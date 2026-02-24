defmodule Toast.Deployment.SingleServerController do
  @moduledoc "GenServer orchestrating the lifecycle of a single-server ArangoDB deployment."

  use GenServer

  require Logger

  alias Toast.Config
  alias Toast.Process.ServerProcess
  alias Toast.Deployment.{Factory, Health, ServerInstance}
  alias Toast.Diagnostics.{CrashLogParser, Sanitizer, ServerLog}
  alias Toast.PortAllocator

  @type status :: :stopped | :starting | :ready | :stopping | :failed

  # --- Client API ---

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts) do
    {name, init_opts} = Keyword.pop(opts, :name)

    if name do
      GenServer.start_link(__MODULE__, init_opts, name: name)
    else
      GenServer.start_link(__MODULE__, init_opts)
    end
  end

  @spec deploy(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def deploy(server, timeout \\ 60_000) do
    GenServer.call(server, {:deploy, timeout}, timeout + 5_000)
  end

  @spec shutdown(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def shutdown(server, timeout \\ 30_000) do
    GenServer.call(server, {:shutdown, timeout}, timeout + 5_000)
  end

  @spec get_status(GenServer.server()) :: status()
  def get_status(server) do
    GenServer.call(server, :get_status)
  end

  @spec get_endpoint(GenServer.server()) :: String.t() | nil
  def get_endpoint(server) do
    GenServer.call(server, :get_endpoint)
  end

  @spec get_info(GenServer.server()) :: map()
  def get_info(server) do
    GenServer.call(server, :get_info)
  end

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    config = Keyword.get(opts, :config, Config.load())
    id = Keyword.get_lazy(opts, :id, &generate_id/0)

    state = %{
      config: config,
      status: :stopped,
      server: %ServerInstance{id: id, role: :single},
      error: nil,
      diagnostics: nil,
      on_crash: Keyword.get(opts, :on_crash),
      on_event: Keyword.get(opts, :on_event)
    }

    {:ok, state}
  end

  @impl true
  def handle_call({:deploy, timeout}, _from, %{status: :stopped} = state) do
    case do_deploy(state, timeout) do
      {:ok, new_state} ->
        {:reply, :ok, new_state}

      {:error, reason, new_state} ->
        {:reply, {:error, reason}, new_state}
    end
  end

  def handle_call({:deploy, _timeout}, _from, state) do
    {:reply, {:error, {:invalid_status, state.status}}, state}
  end

  def handle_call({:shutdown, timeout}, _from, %{status: :ready} = state) do
    new_state = do_shutdown(state, timeout)
    {:reply, :ok, new_state}
  end

  def handle_call({:shutdown, _timeout}, _from, %{status: :stopped} = state) do
    {:reply, :ok, state}
  end

  def handle_call({:shutdown, timeout}, _from, %{status: :failed} = state) do
    new_state = do_cleanup(state, timeout)
    {:reply, :ok, new_state}
  end

  def handle_call({:shutdown, _timeout}, _from, state) do
    {:reply, {:error, {:invalid_status, state.status}}, state}
  end

  def handle_call(:get_status, _from, state) do
    {:reply, state.status, state}
  end

  def handle_call(:get_endpoint, _from, state) do
    {:reply, state.server.endpoint, state}
  end

  def handle_call(:get_info, _from, state) do
    info = %{
      server: state.server,
      status: state.status,
      error: state.error,
      diagnostics: state.diagnostics
    }

    {:reply, info, state}
  end

  def handle_call({:get_server, server_id}, _from, state) do
    if state.server.id == server_id do
      {:reply, state.server, state}
    else
      {:reply, {:error, :not_found}, state}
    end
  end

  def handle_call(:get_servers, _from, state) do
    {:reply, [state.server], state}
  end

  def handle_call({:get_servers, role}, _from, state) do
    servers = if state.server.role == role, do: [state.server], else: []
    {:reply, servers, state}
  end

  @impl true
  def handle_info({:server_crashed, server_id, crash_info}, state) do
    Logger.error("Server #{server_id} crashed: #{inspect(crash_info)}")
    stop_health_monitor(state)
    notify_crash(state.on_crash, crash_info)
    notify_event(state.on_event, {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()})
    state = put_server(%{state | status: :failed, error: {:server_crashed, crash_info}}, health_monitor: nil)
    {:noreply, state}
  end

  def handle_info({:server_unhealthy, server_id}, state) do
    Logger.error("Server #{server_id} is unresponsive, killing process")
    stop_server(state, 5_000)
    crash_info = %{exit_status: nil, signal: nil, timestamp: DateTime.utc_now()}
    notify_crash(state.on_crash, crash_info)
    notify_event(state.on_event, {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()})
    state = put_server(%{state | status: :failed, error: {:server_unhealthy, server_id}}, server_pid: nil, health_monitor: nil)
    {:noreply, state}
  end

  def handle_info({:DOWN, _ref, :process, pid, reason}, state) when reason != :normal do
    if pid == state.server.health_monitor and state.status == :ready do
      Logger.warning("HealthMonitor died unexpectedly (#{inspect(reason)}), restarting")

      case start_health_monitor(state) do
        {:ok, new_pid} -> {:noreply, put_server(state, health_monitor: new_pid)}
        {:error, _} -> {:noreply, state}
      end
    else
      {:noreply, state}
    end
  end

  def handle_info(msg, state) do
    Logger.debug("Unexpected message: #{inspect(msg)}")
    {:noreply, state}
  end

  # --- Deploy sequence ---

  defp do_deploy(state, timeout) do
    id = state.server.id
    Logger.debug("Starting deploy for #{id} (timeout=#{timeout}ms)")
    state = %{state | status: :starting}

    with {:ok, port} <- PortAllocator.allocate(),
         _ = Logger.debug("#{id}: allocated port #{port}"),
         state = put_server(state, port: port, endpoint: "http://127.0.0.1:#{port}"),
         {:ok, launch_spec} <- Factory.build_single_server(state.config, id, port),
         state = put_server(state, log_file: launch_spec.log_file, server_dir: launch_spec.server_dir),
         {:ok, server_pid} <- start_server_process(launch_spec),
         _ = Logger.debug("#{id}: server process started (#{inspect(server_pid)})"),
         :ok <- ServerProcess.launch(server_pid),
         os_pid = ServerProcess.os_pid(server_pid),
         state = put_server(state, server_pid: server_pid, pid: os_pid),
         _ = Logger.info("#{id}: started (os_pid=#{os_pid}), endpoint=#{state.server.endpoint}"),
         _ = notify_event(state.on_event, {:server_started, id, os_pid, DateTime.utc_now()}),
         :ok <- wait_for_ready(state, timeout),
         {:ok, monitor_pid} <- start_health_monitor(state) do
      Logger.info("Deployment #{id} ready at #{state.server.endpoint}")
      {:ok, put_server(%{state | status: :ready}, health_monitor: monitor_pid)}
    else
      {:error, reason} ->
        Logger.error("Deploy failed for #{id}: #{inspect(reason)}")
        failed_state = rollback(state, reason)
        {:error, reason, failed_state}
    end
  end

  defp start_server_process(launch_spec) do
    opts = [
      id: launch_spec.id,
      executable: launch_spec.executable,
      args: launch_spec.args,
      env: launch_spec.env,
      working_dir: launch_spec.working_dir,
      listener: self(),
      output_handler: &print_server_output/2
    ]

    Toast.Process.Supervisor.start_server(opts)
  end

  defp wait_for_ready(state, timeout) do
    process_check_fn = fn -> ServerProcess.status(state.server.server_pid) == :running end

    Health.wait_until_ready(state.server.endpoint,
      timeout: timeout,
      process_check_fn: process_check_fn
    )
  end

  # --- Shutdown sequence ---

  defp do_shutdown(state, timeout) do
    Logger.debug("Shutting down deployment #{state.server.id}")
    state = %{state | status: :stopping}
    do_cleanup(state, timeout)
  end

  defp do_cleanup(state, timeout) do
    Logger.debug("Cleaning up #{state.server.id}")
    stop_health_monitor(state)
    stop_server(state, timeout)
    notify_event(state.on_event, {:server_stopped, state.server.id, state.server.pid, nil, DateTime.utc_now()})
    diagnostics = collect_diagnostics(state)
    Logger.debug("#{state.server.id}: diagnostics collected")
    put_server(%{state | status: :stopped, diagnostics: diagnostics}, server_pid: nil, health_monitor: nil)
  end

  defp collect_diagnostics(%{server: %{server_dir: nil}}), do: nil

  defp collect_diagnostics(state) do
    server = state.server
    sanitizer_errors = Sanitizer.collect_errors(server.server_dir, server.id)
    log_content = Toast.Utils.Filesystem.read_file_or_nil(server.log_file)

    %{
      sanitizer_errors: sanitizer_errors,
      server_log: if(log_content, do: ServerLog.scan(log_content)),
      crash_report: if(log_content, do: CrashLogParser.parse(log_content)),
      server_error: state.error,
      server: server
    }
  end

  defp stop_server(%{server: %{server_pid: nil}}, _timeout), do: :ok

  defp stop_server(%{server: %{server_pid: pid}}, timeout) do
    ServerProcess.stop(pid, timeout)
    DynamicSupervisor.terminate_child(Toast.Process.Supervisor, pid)
  catch
    :exit, _ -> :ok
  end

  # --- Rollback on deploy failure ---

  defp rollback(state, reason) do
    Logger.debug("Rolling back #{state.server.id} due to: #{inspect(reason)}")
    stop_health_monitor(state)
    stop_server(state, 5_000)
    put_server(%{state | status: :failed, error: reason}, server_pid: nil, health_monitor: nil)
  end

  # --- Health monitoring ---

  defp start_health_monitor(state) do
    case Toast.Process.Supervisor.start_health_monitor(
           server_id: state.server.id,
           endpoint: state.server.endpoint,
           listener: self()
         ) do
      {:ok, pid} ->
        Process.monitor(pid)
        {:ok, pid}

      error ->
        error
    end
  end

  defp stop_health_monitor(%{server: %{health_monitor: nil}}), do: :ok

  defp stop_health_monitor(%{server: %{health_monitor: pid}}) do
    Toast.Process.HealthMonitor.stop(pid)
  catch
    :exit, _ -> :ok
  end

  # --- Helpers ---

  defp put_server(state, updates) do
    %{state | server: struct!(state.server, updates)}
  end

  defp notify_crash(nil, _crash_info), do: :ok
  defp notify_crash(on_crash, crash_info) when is_function(on_crash, 1), do: on_crash.(crash_info)

  defp notify_event(nil, _event), do: :ok
  defp notify_event(on_event, event) when is_function(on_event, 1), do: on_event.(event)

  defp print_server_output(server_id, data) do
    data
    |> String.split("\n")
    |> Enum.reject(&(&1 == ""))
    |> Enum.each(&IO.puts("  #{server_id} | #{&1}"))
  end

  defp generate_id do
    "toast-#{System.unique_integer([:positive])}"
  end
end
