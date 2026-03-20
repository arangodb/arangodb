defmodule Toast.Deployment.ServerLifecycle do
  @moduledoc "Shared server lifecycle operations used by the deployment Controller."

  require Logger

  alias Toast.Deployment.{Health, ServerInstance}
  alias Toast.Process.{HealthMonitor, ServerProcess}

  @intentional_exit_signals [nil, 15]

  @base_stop_timeout 30_000
  @base_relaunch_timeout 60_000
  @base_sleep_after_kill 200

  # --- Server control operations ---

  @spec stop_server(ServerInstance.t(), keyword()) :: :ok
  def stop_server(%ServerInstance{} = server, opts \\ []) do
    factor = Keyword.get(opts, :timeout_factor, 1)
    Logger.info("Stopping server #{server.id} (timeout=#{@base_stop_timeout * factor}ms)")
    ServerProcess.stop(server.server_pid, @base_stop_timeout * factor)
    suspend_health_monitor(server)
    :ok
  end

  @spec kill_server(ServerInstance.t()) :: :ok
  def kill_server(%ServerInstance{} = server) do
    Logger.info("Killing server #{server.id}")
    ServerProcess.kill(server.server_pid)
    suspend_health_monitor(server)
    :ok
  end

  @spec abort_server(ServerInstance.t()) :: :ok | {:error, :not_running}
  def abort_server(%ServerInstance{} = server) do
    Logger.info("Aborting server #{server.id} (SIGABRT)")
    suspend_health_monitor(server)
    ServerProcess.send_signal(server.server_pid, :sigabrt)
  end

  @spec pause_server(ServerInstance.t()) :: :ok
  def pause_server(%ServerInstance{} = server) do
    Logger.info("Pausing server #{server.id}")
    ServerProcess.pause(server.server_pid)
    suspend_health_monitor(server)
    :ok
  end

  @spec resume_server(ServerInstance.t()) :: :ok
  def resume_server(%ServerInstance{} = server) do
    Logger.info("Resuming server #{server.id}")
    ServerProcess.resume(server.server_pid)
    resume_health_monitor(server)
    :ok
  end

  @spec stop_before_restart(ServerInstance.t(), keyword()) :: :ok
  def stop_before_restart(%ServerInstance{} = server, opts \\ []) do
    factor = Keyword.get(opts, :timeout_factor, 1)

    Logger.info("Stopping server #{server.id} before restart (state=#{server.operational_state})")

    case server.operational_state do
      :running ->
        ServerProcess.stop(server.server_pid, @base_stop_timeout * factor)
        suspend_health_monitor(server)

      :paused ->
        ServerProcess.kill(server.server_pid)
        Process.sleep(@base_sleep_after_kill * factor)
        suspend_health_monitor(server)

      _stopped_or_crashed ->
        :ok
    end
  end

  @spec relaunch_and_wait(ServerInstance.t(), keyword()) :: :ok | {:error, term()}
  def relaunch_and_wait(%ServerInstance{} = server, opts) do
    Logger.info("Relaunching server #{server.id}")
    factor = Keyword.get(opts, :timeout_factor, 1)
    process_check_fn = fn -> ServerProcess.status(server.server_pid) == :running end

    with :ok <- ServerProcess.relaunch(server.server_pid, opts),
         :ok <-
           Health.wait_until_ready(server.endpoint,
             timeout: @base_relaunch_timeout * factor,
             process_check_fn: process_check_fn
           ) do
      resume_health_monitor(server)
      Logger.info("Server #{server.id} relaunched and ready")
      :ok
    end
  end

  # --- Crash handling ---

  @spec handle_crash(
          String.t(),
          Toast.Process.CrashInfo.t(),
          map(),
          ServerInstance.t() | nil,
          map()
        ) ::
          {:expected, map()}
          | :intentional_exit
          | :crash_during_intentional_stop
          | :unexpected_crash
  def handle_crash(server_id, crash_info, expected_crashes, server, crash_ctx) do
    case Map.get(expected_crashes, server_id) do
      %{timer: _timer} = entry ->
        handle_expected_crash(server_id, crash_info, entry, expected_crashes, crash_ctx)

      nil ->
        handle_unexpected_crash(server_id, crash_info, server, crash_ctx)
    end
  end

  defp handle_expected_crash(server_id, crash_info, entry, expected_crashes, crash_ctx) do
    Logger.info("Server #{server_id} crashed as expected")

    notify_crash_event(crash_ctx, server_id, crash_info, true)

    case entry.waiter do
      {from, verify_timer} ->
        Process.cancel_timer(verify_timer)
        Process.cancel_timer(entry.timer)
        GenServer.reply(from, {:ok, crash_info})
        {:expected, Map.delete(expected_crashes, server_id)}

      nil ->
        entry = %{entry | crash_info: crash_info}
        {:expected, Map.put(expected_crashes, server_id, entry)}
    end
  end

  defp handle_unexpected_crash(
         server_id,
         crash_info,
         %ServerInstance{expecting_exit: true},
         crash_ctx
       ) do
    if crash_info.signal in @intentional_exit_signals do
      Logger.debug(
        "Server #{server_id} exited intentionally (signal=#{inspect(crash_info.signal)})"
      )

      :intentional_exit
    else
      Logger.error(
        "Server #{server_id} crashed unexpectedly during intentional stop: #{inspect(crash_info)}"
      )

      notify_crash_event(crash_ctx, server_id, crash_info, false)
      ToastTest.CrashMonitor.handle_crash(server_id, crash_info)
      :crash_during_intentional_stop
    end
  end

  defp handle_unexpected_crash(server_id, crash_info, _server, crash_ctx) do
    Logger.error("Server #{server_id} crashed: #{inspect(crash_info)}")
    notify_crash_event(crash_ctx, server_id, crash_info, false)
    ToastTest.CrashMonitor.handle_crash(server_id, crash_info)
    :unexpected_crash
  end

  defp notify_crash_event(crash_ctx, server_id, crash_info, expected) do
    ToastTest.ProcessHistory.notify(%{
      event: :server_crashed,
      deployment_id: crash_ctx.deployment_id,
      server_id: server_id,
      pid: crash_info.os_pid,
      crash_info: crash_info,
      expected: expected,
      timestamp: DateTime.utc_now()
    })
  end

  # --- Expect / verify crash protocol ---

  @spec expect_crash(String.t(), timeout(), map(), ServerInstance.t()) ::
          {:ok, map()} | {:error, :already_expected}
  def expect_crash(server_id, _timeout, expected_crashes, %ServerInstance{})
      when is_map_key(expected_crashes, server_id) do
    {:error, :already_expected}
  end

  def expect_crash(server_id, timeout, expected_crashes, %ServerInstance{} = server) do
    Logger.debug("Registered expected crash for #{server_id} (timeout=#{timeout}ms)")
    suspend_health_monitor(server)
    timer = Process.send_after(self(), {:expect_crash_timeout, server_id}, timeout)
    entry = %{timer: timer, crash_info: nil, waiter: nil}
    {:ok, Map.put(expected_crashes, server_id, entry)}
  end

  @spec verify_crash(String.t(), timeout(), map(), GenServer.from()) ::
          {:reply, term(), map()} | {:noreply, map()}
  def verify_crash(server_id, timeout, expected_crashes, from) do
    case Map.get(expected_crashes, server_id) do
      nil ->
        {:reply, {:error, :no_expectation}, expected_crashes}

      %{crash_info: nil} = entry ->
        verify_timer = Process.send_after(self(), {:verify_crash_timeout, server_id}, timeout)
        entry = %{entry | waiter: {from, verify_timer}}
        {:noreply, Map.put(expected_crashes, server_id, entry)}

      %{crash_info: crash_info, timer: timer} ->
        Process.cancel_timer(timer)
        {:reply, {:ok, crash_info}, Map.delete(expected_crashes, server_id)}
    end
  end

  @spec handle_expect_crash_timeout(String.t(), map(), ServerInstance.t() | nil) :: map()
  def handle_expect_crash_timeout(server_id, expected_crashes, server) do
    case Map.get(expected_crashes, server_id) do
      %{crash_info: nil} = entry ->
        Logger.warning("Expected crash for #{server_id} timed out")
        notify_waiter_timeout(entry)
        resume_health_monitor(server)
        Map.delete(expected_crashes, server_id)

      _ ->
        expected_crashes
    end
  end

  defp notify_waiter_timeout(%{waiter: {from, verify_timer}}) do
    Process.cancel_timer(verify_timer)
    GenServer.reply(from, {:error, :timeout})
  end

  defp notify_waiter_timeout(_), do: :ok

  @spec handle_verify_crash_timeout(String.t(), map(), ServerInstance.t() | nil) :: map()
  def handle_verify_crash_timeout(server_id, expected_crashes, server) do
    case Map.get(expected_crashes, server_id) do
      %{waiter: {from, _}} ->
        GenServer.reply(from, {:error, :timeout})
        resume_health_monitor(server)
        Map.delete(expected_crashes, server_id)

      _ ->
        expected_crashes
    end
  end

  # --- State validation ---

  @spec require_state(ServerInstance.t(), atom()) :: :ok | {:error, {:unexpected_state, atom()}}
  def require_state(%ServerInstance{} = server, expected) do
    if server.operational_state == expected,
      do: :ok,
      else: {:error, {:unexpected_state, server.operational_state}}
  end

  @spec require_state_in(ServerInstance.t(), [atom()]) ::
          :ok | {:error, {:unexpected_state, atom()}}
  def require_state_in(%ServerInstance{} = server, expected_list) do
    if server.operational_state in expected_list,
      do: :ok,
      else: {:error, {:unexpected_state, server.operational_state}}
  end

  # --- Health monitor helpers ---

  @spec suspend_health_monitor(ServerInstance.t()) :: :ok
  def suspend_health_monitor(%{health_monitor: nil}), do: :ok

  def suspend_health_monitor(%{health_monitor: pid}),
    do: HealthMonitor.suspend(pid)

  @spec resume_health_monitor(ServerInstance.t() | nil) :: :ok
  def resume_health_monitor(nil), do: :ok
  def resume_health_monitor(%{health_monitor: nil}), do: :ok

  def resume_health_monitor(%{health_monitor: pid}),
    do: HealthMonitor.resume(pid)

  @spec stop_health_monitor(ServerInstance.t()) :: :ok
  def stop_health_monitor(%{health_monitor: nil}), do: :ok

  def stop_health_monitor(%{health_monitor: pid}) do
    HealthMonitor.stop(pid)
  catch
    :exit, _ -> :ok
  end

  # --- Server output ---

  @spec print_server_output(String.t(), String.t()) :: :ok
  def print_server_output(server_id, data) do
    data
    |> String.split("\n", trim: true)
    |> Enum.each(&IO.puts("  #{server_id} | #{&1}"))
  end
end
