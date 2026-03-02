defmodule Toast.Deployment.Controller.SingleServer do
  @moduledoc false

  @behaviour Toast.Deployment.Controller

  require Logger

  alias Toast.Process.ServerProcess
  alias Toast.Deployment.{Factory, Health, ServerInstance, ServerLifecycle}
  alias Toast.Deployment.Controller
  alias Toast.Diagnostics
  alias Toast.PortAllocator

  @impl true
  def init_mode_state, do: %{}

  @impl true
  def init_servers(id), do: %{id => %ServerInstance{id: id, role: :single}}

  @impl true
  def deploy(state, timeout) do
    id = state.id
    Logger.debug("Starting deploy for #{id} (timeout=#{timeout}ms)")
    state = %{state | status: :starting}

    with {:ok, port} <- PortAllocator.allocate(),
         _ = Logger.debug("#{id}: allocated port #{port}"),
         state = update_server_fields(state, id, port: port, endpoint: "http://127.0.0.1:#{port}"),
         {:ok, launch_spec} <- Factory.build_single_server(state.config, id, port),
         state =
           update_server_fields(state, id,
             log_file: launch_spec.log_file,
             server_dir: launch_spec.server_dir,
             launch_spec: launch_spec
           ),
         {:ok, server_pid} <- start_server_process(launch_spec),
         _ = Logger.debug("#{id}: server process started (#{inspect(server_pid)})"),
         :ok <- ServerProcess.launch(server_pid),
         os_pid = ServerProcess.os_pid(server_pid),
         state = update_server_fields(state, id, server_pid: server_pid, pid: os_pid),
         _ =
           Logger.info(
             "#{id}: started (os_pid=#{os_pid}), endpoint=#{state.servers[id].endpoint}"
           ),
         _ =
           ServerLifecycle.notify_event(
             state.on_event,
             {:server_started, id, os_pid, DateTime.utc_now()}
           ),
         :ok <- wait_for_ready(state, id, timeout),
         {:ok, monitor_pid} <-
           Controller.start_single_health_monitor(id, state.servers[id].endpoint) do
      Logger.info("Deployment #{id} ready at #{state.servers[id].endpoint}")

      state =
        update_server_fields(state, id,
          health_monitor: monitor_pid,
          operational_state: :running
        )

      {:ok, %{state | status: :ready}}
    else
      {:error, reason} ->
        Logger.error("Deploy failed for #{id}: #{inspect(reason)}")
        failed_state = rollback(state, reason)
        {:error, reason, failed_state}
    end
  end

  @impl true
  def shutdown(%{status: :failed} = state, timeout) do
    do_cleanup(state, timeout)
  end

  def shutdown(state, timeout) do
    Logger.debug("Shutting down deployment #{state.id}")
    state = %{state | status: :stopping}
    do_cleanup(state, timeout)
  end

  @impl true
  def derive_status(servers) do
    case Map.values(servers) do
      [server] ->
        cond do
          server.operational_state == :running -> :ready
          ServerInstance.unexpected_crash?(server) -> :failed
          true -> :degraded
        end

      _ ->
        :ready
    end
  end

  @impl true
  def resolve_target(state, server_id) when is_binary(server_id) do
    if Map.has_key?(state.servers, server_id),
      do: {:ok, [server_id]},
      else: {:error, :not_found}
  end

  def resolve_target(state, role: :single) do
    case Map.keys(state.servers) do
      [id] -> {:ok, [id]}
      _ -> {:error, {:no_servers_for_role, :single}}
    end
  end

  def resolve_target(_state, role: role), do: {:error, {:no_servers_for_role, role}}
  def resolve_target(_state, target), do: {:error, {:invalid_target, target}}

  @impl true
  def build_info(state) do
    server =
      case Map.values(state.servers) do
        [s] -> s
        _ -> nil
      end

    primary_endpoint = if server, do: server.endpoint

    %{
      id: state.id,
      status: state.status,
      servers: state.servers,
      error: state.error,
      diagnostics: state.diagnostics,
      primary_endpoint: primary_endpoint
    }
  end

  # --- Private helpers ---

  defp start_server_process(launch_spec) do
    opts = [
      id: launch_spec.id,
      executable: launch_spec.executable,
      args: launch_spec.args,
      env: launch_spec.env,
      working_dir: launch_spec.working_dir,
      listener: self(),
      output_handler: &ServerLifecycle.print_server_output/2
    ]

    Toast.Process.Supervisor.start_server(opts)
  end

  defp wait_for_ready(state, id, timeout) do
    server = state.servers[id]
    process_check_fn = fn -> ServerProcess.status(server.server_pid) == :running end

    Health.wait_until_ready(server.endpoint,
      timeout: timeout,
      process_check_fn: process_check_fn
    )
  end

  defp do_cleanup(state, timeout) do
    Logger.debug("Cleaning up #{state.id}")
    Controller.stop_all_health_monitors(state)

    for {server_id, _server} <- state.servers do
      Controller.stop_server_process(state, server_id, timeout)
    end

    for {server_id, server} <- state.servers do
      ServerLifecycle.notify_event(
        state.on_event,
        {:server_stopped, server_id, server.pid, nil, DateTime.utc_now()}
      )
    end

    diagnostics = collect_diagnostics(state)
    Logger.debug("#{state.id}: diagnostics collected")

    %{
      state
      | status: :stopped,
        servers: Controller.clear_server_pids(state.servers),
        diagnostics: diagnostics
    }
  end

  defp collect_diagnostics(state) do
    case Map.values(state.servers) do
      [%{server_dir: nil}] ->
        nil

      [server] ->
        %{server.id => Diagnostics.build_server_diagnostics(server, state.error)}

      _ ->
        nil
    end
  end

  defp rollback(state, reason) do
    Logger.debug("Rolling back #{state.id} due to: #{inspect(reason)}")
    Controller.stop_all_health_monitors(state)

    for {server_id, _server} <- state.servers do
      Controller.stop_server_process(state, server_id, 5_000 * state.config.timeout_factor)
    end

    %{
      state
      | status: :failed,
        servers: Controller.clear_server_pids(state.servers),
        error: reason
    }
  end

  defp update_server_fields(state, server_id, updates) do
    Controller.update_server(state, server_id, updates)
  end
end
