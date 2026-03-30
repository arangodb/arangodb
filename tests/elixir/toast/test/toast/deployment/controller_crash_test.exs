defmodule Toast.Deployment.ControllerCrashTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.{Config, Controller, ServerInstance}
  alias Toast.Process.CrashInfo

  defp start_controller do
    id = "crash-test-#{System.unique_integer([:positive])}"
    {:ok, ctrl} = Controller.start_link(config: Config.new(), id: id)

    on_exit(fn ->
      try do
        GenServer.stop(ctrl)
      catch
        :exit, _ -> :ok
      end
    end)

    ctrl
  end

  defp inject_server(ctrl, overrides \\ []) do
    :sys.replace_state(ctrl, fn state ->
      defaults = [id: state.id, role: :single, operational_state: :running, expecting_exit: false]
      server = struct!(ServerInstance, Keyword.merge(defaults, overrides))
      %{state | status: :ready, servers: Map.put(state.servers, server.id, server)}
    end)

    :sys.get_state(ctrl).id
  end

  defp crash_info(overrides \\ []) do
    defaults = [
      exit_status: 139,
      signal: 11,
      timestamp: :os.system_time(:microsecond)
    ]

    struct!(CrashInfo, Keyword.merge(defaults, overrides))
  end

  describe "handle_crash classification via :server_crashed message" do
    test "unexpected crash sets status to :failed" do
      ctrl = start_controller()
      server_id = inject_server(ctrl)

      send(ctrl, {:server_crashed, server_id, crash_info()})
      Process.sleep(50)

      state = :sys.get_state(ctrl)
      assert state.status == :failed
      assert {:server_crashed, ^server_id, _} = state.error
      assert state.servers[server_id].operational_state == :crashed
    end

    test "intentional exit (nil signal) does not change status" do
      ctrl = start_controller()
      server_id = inject_server(ctrl, expecting_exit: true)

      send(ctrl, {:server_crashed, server_id, crash_info(signal: nil)})
      Process.sleep(50)

      state = :sys.get_state(ctrl)
      assert state.status == :ready
      assert state.error == nil
    end

    test "intentional exit (SIGTERM signal 15) does not change status" do
      ctrl = start_controller()
      server_id = inject_server(ctrl, expecting_exit: true)

      send(ctrl, {:server_crashed, server_id, crash_info(signal: 15)})
      Process.sleep(50)

      state = :sys.get_state(ctrl)
      assert state.status == :ready
      assert state.error == nil
    end

    test "crash during intentional stop (SIGSEGV) sets status to :failed" do
      ctrl = start_controller()
      server_id = inject_server(ctrl, expecting_exit: true)

      send(ctrl, {:server_crashed, server_id, crash_info(signal: 11)})
      Process.sleep(50)

      state = :sys.get_state(ctrl)
      assert state.status == :failed
      assert {:server_crashed, ^server_id, _} = state.error
    end

    test "crash during intentional stop (SIGABRT signal 6) sets status to :failed" do
      ctrl = start_controller()
      server_id = inject_server(ctrl, expecting_exit: true)

      send(ctrl, {:server_crashed, server_id, crash_info(signal: 6)})
      Process.sleep(50)

      state = :sys.get_state(ctrl)
      assert state.status == :failed
    end

    test "expected crash (no waiter) stores crash_info and derives status" do
      ctrl = start_controller()
      server_id = inject_server(ctrl)

      :ok = GenServer.call(ctrl, {:expect_crash, server_id, 5_000})

      info = crash_info()
      send(ctrl, {:server_crashed, server_id, info})
      Process.sleep(50)

      state = :sys.get_state(ctrl)
      assert state.servers[server_id].operational_state == :crashed
      # Expected crash — status is derived, not forced to :failed
      assert state.status in [:degraded, :failed]
    end

    test "expected crash with pending verify_crash replies to waiter" do
      ctrl = start_controller()
      server_id = inject_server(ctrl)

      :ok = GenServer.call(ctrl, {:expect_crash, server_id, 5_000})

      # Start verify_crash in a task (it will block waiting for crash)
      task =
        Task.async(fn ->
          GenServer.call(ctrl, {:verify_crash, server_id, 5_000}, 10_000)
        end)

      # Give verify_crash time to register waiter
      Process.sleep(50)

      # Simulate crash
      info = crash_info()
      send(ctrl, {:server_crashed, server_id, info})

      # verify_crash should return the crash info
      assert {:ok, returned_info} = Task.await(task, 5_000)
      assert returned_info.signal == 11
      assert returned_info.exit_status == 139

      # Expectation should be cleaned up
      state = :sys.get_state(ctrl)
      assert state.expected_crashes == %{}
    end
  end
end
