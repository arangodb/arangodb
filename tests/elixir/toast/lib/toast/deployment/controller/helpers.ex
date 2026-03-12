defmodule Toast.Deployment.Controller.Helpers do
  @moduledoc false

  require Logger

  alias Toast.Deployment.{ServerInstance, ServerLifecycle}
  alias Toast.Process.ServerProcess
  alias Toast.Process.Supervisor, as: ProcessSupervisor

  @spec fetch_server(Toast.Deployment.Controller.State.t(), String.t()) ::
          {:ok, ServerInstance.t()} | {:error, :not_found}
  def fetch_server(state, server_id) do
    with :error <- Map.fetch(state.servers, server_id), do: {:error, :not_found}
  end

  @spec update_server(Toast.Deployment.Controller.State.t(), String.t(), keyword()) ::
          Toast.Deployment.Controller.State.t()
  def update_server(state, server_id, updates) do
    %{state | servers: Map.update!(state.servers, server_id, &struct!(&1, updates))}
  end

  @spec start_single_health_monitor(String.t(), String.t()) :: {:ok, pid()} | {:error, term()}
  def start_single_health_monitor(server_id, endpoint) do
    case ProcessSupervisor.start_health_monitor(
           server_id: server_id,
           endpoint: endpoint,
           listener: self()
         ) do
      {:ok, pid} ->
        Process.monitor(pid)
        {:ok, pid}

      error ->
        error
    end
  end

  @spec stop_all_health_monitors(Toast.Deployment.Controller.State.t()) :: :ok
  def stop_all_health_monitors(state) do
    for {_id, server} <- state.servers do
      ServerLifecycle.stop_health_monitor(server)
    end

    :ok
  end

  @spec stop_health_monitor(Toast.Deployment.Controller.State.t(), String.t()) :: :ok
  def stop_health_monitor(state, server_id) do
    case state.servers[server_id] do
      nil -> :ok
      server -> ServerLifecycle.stop_health_monitor(server)
    end
  end

  @spec stop_server_process(Toast.Deployment.Controller.State.t(), String.t(), timeout()) :: :ok
  def stop_server_process(state, server_id, timeout) do
    Logger.debug("Stopping server process for #{server_id}")

    case state.servers[server_id] do
      %{server_pid: pid} when pid != nil ->
        ServerProcess.stop(pid, timeout)
        DynamicSupervisor.terminate_child(ProcessSupervisor, pid)
        :ok

      _ ->
        :ok
    end
  catch
    :exit, _ -> :ok
  end

  @spec spec_to_server_opts(map()) :: keyword()
  def spec_to_server_opts(spec) do
    [
      id: spec.id,
      executable: spec.executable,
      args: spec.args,
      env: spec.env,
      working_dir: spec.working_dir,
      listener: self(),
      output_handler: &ServerLifecycle.print_server_output/2
    ]
  end

  @spec clear_server_pids(map()) :: map()
  def clear_server_pids(servers) do
    Map.new(servers, fn {id, server} -> {id, %{server | server_pid: nil, health_monitor: nil}} end)
  end

  @abort_timeout 5_000

  @doc """
  Send SIGABRT to all servers that have a running process.

  Used before rollback during deploy failure to get crash backtraces
  from all servers. Waits for the aborted processes to terminate by
  receiving `{:server_crashed, ...}` messages from `ServerProcess`
  (up to `@abort_timeout` ms). Does NOT register expected crashes
  (the crashes during rollback are handled by the stopping flow).

  Must be called from the Controller process (the listener for crash
  notifications).
  """
  @spec abort_all_servers(Toast.Deployment.Controller.State.t()) :: :ok
  def abort_all_servers(state) do
    servers_with_pids =
      Enum.filter(state.servers, fn {_id, server} -> server.server_pid != nil end)

    Logger.info("Aborting #{length(servers_with_pids)} server(s) for crash backtrace")

    aborted_ids =
      Enum.flat_map(servers_with_pids, fn {server_id, server} ->
        case ServerProcess.send_signal(server.server_pid, :sigabrt) do
          :ok ->
            Logger.info("Sent SIGABRT to #{server_id} for crash backtrace")
            [server_id]

          {:error, :not_running} ->
            []
        end
      end)

    await_crashes(Map.new(aborted_ids, &{&1, true}), @abort_timeout)
    Logger.debug("All abort crash notifications received")
    :ok
  end

  defp await_crashes(remaining, _timeout) when map_size(remaining) == 0, do: :ok

  defp await_crashes(remaining, timeout) do
    deadline = System.monotonic_time(:millisecond) + timeout
    do_await_crashes(remaining, deadline)
  end

  defp do_await_crashes(remaining, _deadline) when map_size(remaining) == 0, do: :ok

  defp do_await_crashes(remaining, deadline) do
    timeout = max(0, deadline - System.monotonic_time(:millisecond))

    receive do
      {:server_crashed, server_id, _crash_info} when is_map_key(remaining, server_id) ->
        do_await_crashes(Map.delete(remaining, server_id), deadline)
    after
      timeout -> :ok
    end
  end

  @spec remaining_ms(integer()) :: non_neg_integer()
  def remaining_ms(deadline) do
    max(0, deadline - System.monotonic_time(:millisecond))
  end

  @spec resolve_target_by_id(Toast.Deployment.Controller.State.t(), String.t()) ::
          {:ok, [String.t()]} | {:error, :not_found}
  def resolve_target_by_id(state, server_id) do
    if Map.has_key?(state.servers, server_id),
      do: {:ok, [server_id]},
      else: {:error, :not_found}
  end
end
