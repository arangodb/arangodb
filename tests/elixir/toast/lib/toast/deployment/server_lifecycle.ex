defmodule Toast.Deployment.ServerLifecycle do
  @moduledoc "Shared server lifecycle operations used by both ClusterController and SingleServerController."

  require Logger

  alias Toast.Process.ServerProcess
  alias Toast.Deployment.{Health, ServerInstance}

  @intentional_exit_signals [nil, 15]

  # --- Server control operations ---

  @spec stop_server(ServerInstance.t()) :: :ok
  def stop_server(%ServerInstance{} = server) do
    ServerProcess.stop(server.server_pid, 30_000)
    suspend_health_monitor(server)
    :ok
  end

  @spec kill_server(ServerInstance.t()) :: :ok
  def kill_server(%ServerInstance{} = server) do
    ServerProcess.kill(server.server_pid)
    suspend_health_monitor(server)
    :ok
  end

  @spec pause_server(ServerInstance.t()) :: :ok
  def pause_server(%ServerInstance{} = server) do
    ServerProcess.pause(server.server_pid)
    suspend_health_monitor(server)
    :ok
  end

  @spec resume_server(ServerInstance.t()) :: :ok
  def resume_server(%ServerInstance{} = server) do
    ServerProcess.resume(server.server_pid)
    resume_health_monitor(server)
    :ok
  end

  @spec stop_before_restart(ServerInstance.t()) :: :ok
  def stop_before_restart(%ServerInstance{} = server) do
    case server.operational_state do
      :running ->
        ServerProcess.stop(server.server_pid, 30_000)
        suspend_health_monitor(server)

      :paused ->
        ServerProcess.kill(server.server_pid)
        Process.sleep(200)
        suspend_health_monitor(server)

      _stopped_or_crashed ->
        :ok
    end
  end

  @spec relaunch_and_wait(ServerInstance.t(), keyword()) :: :ok | {:error, term()}
  def relaunch_and_wait(%ServerInstance{} = server, opts) do
    case ServerProcess.relaunch(server.server_pid, opts) do
      :ok ->
        process_check_fn = fn -> ServerProcess.status(server.server_pid) == :running end

        case Health.wait_until_ready(server.endpoint,
               timeout: 60_000,
               process_check_fn: process_check_fn
             ) do
          :ok ->
            resume_health_monitor(server)
            :ok

          {:error, _} = err ->
            err
        end

      {:error, _} = err ->
        err
    end
  end

  # --- Crash handling ---

  @spec handle_crash(String.t(), map(), map(), ServerInstance.t() | nil, map()) ::
          {:expected, map()} | :intentional_exit | :crash_during_intentional_stop | :unexpected_crash
  def handle_crash(server_id, crash_info, expected_crashes, server, on_crash_ctx) do
    case Map.get(expected_crashes, server_id) do
      %{timer: _timer} = entry ->
        handle_expected_crash(server_id, crash_info, entry, expected_crashes, on_crash_ctx)

      nil ->
        handle_unexpected_crash(server_id, crash_info, server, on_crash_ctx)
    end
  end

  defp handle_expected_crash(server_id, crash_info, entry, expected_crashes, on_crash_ctx) do
    Logger.info("Server #{server_id} crashed as expected")
    entry = %{entry | crash_info: crash_info}
    expected_crashes = Map.put(expected_crashes, server_id, entry)
    notify_event(on_crash_ctx.on_event, {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()})

    {:expected, expected_crashes}
  end

  defp handle_unexpected_crash(server_id, crash_info, nil, on_crash_ctx) do
    Logger.error("Server #{server_id} crashed: #{inspect(crash_info)}")
    notify_crash(on_crash_ctx.on_crash, on_crash_ctx.deployment, crash_info)
    notify_event(on_crash_ctx.on_event, {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()})
    :unexpected_crash
  end

  defp handle_unexpected_crash(server_id, crash_info, %ServerInstance{intentional: true} = _server, on_crash_ctx) do
    if crash_info.signal in @intentional_exit_signals do
      Logger.debug("Server #{server_id} exited intentionally (signal=#{inspect(crash_info.signal)})")
      :intentional_exit
    else
      Logger.error("Server #{server_id} crashed unexpectedly during intentional stop: #{inspect(crash_info)}")
      notify_crash(on_crash_ctx.on_crash, on_crash_ctx.deployment, crash_info)
      notify_event(on_crash_ctx.on_event, {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()})
      :crash_during_intentional_stop
    end
  end

  defp handle_unexpected_crash(server_id, crash_info, %ServerInstance{} = _server, on_crash_ctx) do
    Logger.error("Server #{server_id} crashed: #{inspect(crash_info)}")
    notify_crash(on_crash_ctx.on_crash, on_crash_ctx.deployment, crash_info)
    notify_event(on_crash_ctx.on_event, {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()})
    :unexpected_crash
  end

  # --- Expect / verify crash protocol ---

  @spec expect_crash(String.t(), timeout(), map(), ServerInstance.t()) ::
          {:ok, map()} | {:error, :already_expected}
  def expect_crash(server_id, timeout, expected_crashes, %ServerInstance{} = server) do
    if Map.has_key?(expected_crashes, server_id) do
      {:error, :already_expected}
    else
      suspend_health_monitor(server)
      timer = Process.send_after(self(), {:expect_crash_timeout, server_id}, timeout)
      entry = %{timer: timer, crash_info: nil}
      {:ok, Map.put(expected_crashes, server_id, entry)}
    end
  end

  @spec verify_crash(String.t(), timeout(), map(), GenServer.from()) ::
          {:reply, term(), map()} | {:noreply, map()}
  def verify_crash(server_id, timeout, expected_crashes, from) do
    case Map.get(expected_crashes, server_id) do
      nil ->
        {:reply, {:error, :no_expectation}, expected_crashes}

      %{crash_info: nil} ->
        deadline = System.monotonic_time(:millisecond) + timeout
        Process.send_after(self(), {:verify_crash_check, server_id, from, deadline}, 100)
        {:noreply, expected_crashes}

      %{crash_info: crash_info, timer: timer} ->
        Process.cancel_timer(timer)
        {:reply, {:ok, crash_info}, Map.delete(expected_crashes, server_id)}
    end
  end

  @spec handle_expect_crash_timeout(String.t(), map(), ServerInstance.t() | nil) :: map()
  def handle_expect_crash_timeout(server_id, expected_crashes, server) do
    case Map.get(expected_crashes, server_id) do
      %{crash_info: nil} ->
        Logger.warning("Expected crash for #{server_id} timed out")
        if server, do: resume_health_monitor(server)
        Map.delete(expected_crashes, server_id)

      _ ->
        expected_crashes
    end
  end

  @spec handle_verify_crash_check(String.t(), GenServer.from(), integer(), map(), ServerInstance.t() | nil) ::
          {:wait, map()} | {:done, map()}
  def handle_verify_crash_check(server_id, from, deadline, expected_crashes, server) do
    case Map.get(expected_crashes, server_id) do
      %{crash_info: nil} ->
        if System.monotonic_time(:millisecond) < deadline do
          Process.send_after(self(), {:verify_crash_check, server_id, from, deadline}, 100)
          {:wait, expected_crashes}
        else
          if server, do: resume_health_monitor(server)
          GenServer.reply(from, {:error, :timeout})
          {:done, Map.delete(expected_crashes, server_id)}
        end

      %{crash_info: crash_info, timer: timer} ->
        Process.cancel_timer(timer)
        GenServer.reply(from, {:ok, crash_info})
        {:done, Map.delete(expected_crashes, server_id)}

      nil ->
        GenServer.reply(from, {:error, :no_expectation})
        {:done, expected_crashes}
    end
  end

  # --- State validation ---

  @spec require_state(ServerInstance.t(), atom()) :: :ok | {:error, {:unexpected_state, atom()}}
  def require_state(%ServerInstance{} = server, expected) do
    if server.operational_state == expected,
      do: :ok,
      else: {:error, {:unexpected_state, server.operational_state}}
  end

  @spec require_state_in(ServerInstance.t(), [atom()]) :: :ok | {:error, {:unexpected_state, atom()}}
  def require_state_in(%ServerInstance{} = server, expected_list) do
    if server.operational_state in expected_list,
      do: :ok,
      else: {:error, {:unexpected_state, server.operational_state}}
  end

  # --- Health monitor helpers ---

  def suspend_health_monitor(%{health_monitor: nil}), do: :ok

  def suspend_health_monitor(%{health_monitor: pid}),
    do: Toast.Process.HealthMonitor.suspend(pid)

  def resume_health_monitor(%{health_monitor: nil}), do: :ok

  def resume_health_monitor(%{health_monitor: pid}),
    do: Toast.Process.HealthMonitor.resume(pid)

  def stop_health_monitor(%{health_monitor: nil}), do: :ok

  def stop_health_monitor(%{health_monitor: pid}) do
    Toast.Process.HealthMonitor.stop(pid)
  catch
    :exit, _ -> :ok
  end

  # --- Notification helpers ---

  def notify_event(nil, _event), do: :ok
  def notify_event(on_event, event) when is_function(on_event, 1), do: on_event.(event)

  def notify_crash(nil, _deployment, _crash_info), do: :ok

  def notify_crash(on_crash, deployment, crash_info) when is_function(on_crash, 2) do
    on_crash.(deployment, crash_info)
  end

  # --- Server output ---

  def print_server_output(server_id, data) do
    data
    |> String.split("\n")
    |> Enum.reject(&(&1 == ""))
    |> Enum.each(&IO.puts("  #{server_id} | #{&1}"))
  end
end
