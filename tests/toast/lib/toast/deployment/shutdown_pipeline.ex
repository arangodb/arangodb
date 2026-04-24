defmodule Toast.Deployment.ShutdownPipeline do
  @moduledoc "Ordered shutdown, rollback, and abort sequences for a deployment."

  require Logger

  alias Toast.Deployment.{CrashExpectation, Events, ServerInstance, ServerLifecycle}
  alias Toast.Deployment.Controller.State
  alias Toast.Process.ServerProcess
  alias Toast.Process.Supervisor, as: ProcessSupervisor

  @abort_timeout 60_000

  # --- Public API ---

  @spec shutdown(State.t(), timeout()) :: State.t()
  def shutdown(%{status: :failed} = state, timeout) do
    shutdown_servers(state, timeout)
  end

  def shutdown(state, timeout) do
    Logger.debug("Shutting down deployment #{state.id}")
    shutdown_servers(%{state | status: :stopping}, timeout)
  end

  @spec rollback(State.t(), term()) :: State.t()
  def rollback(state, reason) do
    Logger.debug("Rolling back #{state.id} due to: #{inspect(reason)}")
    stop_all_health_monitors(state)
    all_ids = Map.keys(state.servers)
    stop_server_group(all_ids, state, state.config.shutdown_timeout)
    Logger.debug("Rollback complete for #{state.id}")

    %{
      state
      | status: :failed,
        servers: clear_server_pids(state.servers),
        error: reason
    }
  end

  @spec abort_all(State.t()) :: {[map()], State.t()}
  def abort_all(state) do
    running_servers =
      Enum.filter(state.servers, fn {_id, server} ->
        server.operational_state in [:running, :paused]
      end)

    expected_crashes =
      Enum.reduce(running_servers, state.expected_crashes, fn {server_id, server}, acc ->
        if is_map_key(acc, server_id) do
          acc
        else
          ServerLifecycle.suspend_health_monitor(server)
          timer = Process.send_after(self(), {:expect_crash_timeout, server_id}, @abort_timeout)
          Map.put(acc, server_id, %CrashExpectation{timer: timer})
        end
      end)

    killed_servers =
      Enum.map(running_servers, fn {server_id, server} ->
        ServerLifecycle.abort_server(server)

        %{server_id: server_id, os_pid: server.pid, log_file: server.log_file}
      end)

    {killed_servers, %{state | expected_crashes: expected_crashes}}
  end

  @spec handle_deploy_failure(State.t(), term()) :: State.t()
  def handle_deploy_failure(state, reason) do
    Logger.error("Deploy failed for #{state.id}: #{inspect(reason)}")
    {killed_servers, state} = abort_all(state)

    if reason == :timeout do
      state.event_listener.on_event(%{
        event: :timeout_kill,
        deployment_id: state.id,
        source: :startup_timeout,
        reason: "Startup timeout — deployment did not become ready in time",
        servers: killed_servers,
        timestamp: Toast.get_timestamp()
      })
    end

    rollback(state, reason)
  end

  # --- Internal ---

  defp shutdown_servers(state, timeout) do
    deadline = System.monotonic_time(:millisecond) + timeout
    stop_all_health_monitors(state)

    grouped = Enum.group_by(state.servers, fn {_id, s} -> s.role end)

    escalated =
      Enum.flat_map(Enum.reverse(State.role_deploy_order()), fn role ->
        shutdown_role_group(grouped, role, state, deadline)
      end)

    record_shutdown_escalations(state.id, state.event_listener, escalated)

    Events.notify(state.event_listener, state, :deployment_stopped)

    %{state | status: :stopped, servers: clear_server_pids(state.servers)}
  end

  defp shutdown_role_group(grouped, role, state, deadline) do
    case Map.get(grouped, role) do
      nil ->
        []

      servers ->
        ids = Enum.map(servers, fn {id, _} -> id end)
        Logger.debug("#{state.id}: stopping #{role}s")
        stop_server_group(ids, state, State.remaining_ms(deadline))
    end
  end

  defp stop_server_group(server_ids, state, timeout) do
    Task.async_stream(
      server_ids,
      &stop_server_process(state, &1, timeout),
      ordered: false,
      timeout: timeout + ServerProcess.escalation_overhead() + State.task_stream_buffer()
    )
    |> Enum.flat_map(fn
      {:ok, {:escalated, info}} -> [info]
      _ -> []
    end)
  end

  defp stop_server_process(state, server_id, timeout) do
    Logger.debug("Stopping server process for #{server_id}")

    case server = Map.fetch!(state.servers, server_id) do
      %ServerInstance{server_pid: pid} when pid != nil ->
        result = ServerProcess.stop(pid, timeout)
        DynamicSupervisor.terminate_child(ProcessSupervisor, pid)
        Events.server_stopped(state.event_listener, server_id, server, state.id)

        case result do
          :escalated ->
            {:escalated, %{server_id: server_id, os_pid: server.pid, log_file: server.log_file}}

          _ ->
            :ok
        end

      _ ->
        :ok
    end
  catch
    :exit, _ -> :ok
  end

  defp stop_all_health_monitors(state) do
    for {_id, server} <- state.servers do
      ServerLifecycle.stop_health_monitor(server)
    end

    :ok
  end

  defp clear_server_pids(servers) do
    Map.new(servers, fn {id, server} -> {id, %{server | server_pid: nil, health_monitor: nil}} end)
  end

  defp record_shutdown_escalations(_id, _listener, []), do: :ok

  defp record_shutdown_escalations(id, listener, escalated) do
    Logger.warning("#{id}: #{length(escalated)} server(s) required shutdown escalation")

    listener.on_event(%{
      event: :timeout_kill,
      deployment_id: id,
      source: :shutdown_timeout,
      reason: "Shutdown timeout — server(s) did not respond to SIGTERM",
      servers: escalated,
      timestamp: Toast.get_timestamp()
    })
  end
end
