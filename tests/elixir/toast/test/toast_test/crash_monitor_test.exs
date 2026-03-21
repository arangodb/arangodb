defmodule ToastTest.CrashMonitorTest do
  use ExUnit.Case, async: false

  alias ToastTest.{Abort, CrashMonitor}

  setup do
    Abort.clear!()
    on_exit(fn -> Abort.clear!() end)
    :ok
  end

  test "handle_crash sets abort with signal and exit_status" do
    CrashMonitor.handle_crash(
      "test-server",
      %Toast.Process.CrashInfo{
        signal: 11,
        exit_status: 139,
        timestamp: :os.system_time(:microsecond)
      }
    )

    assert {:crash, msg} = Abort.reason()
    assert msg =~ "Server crashed"
    assert msg =~ "signal: 11"
    assert msg =~ "exit_status=139"
  end

  test "handle_crash with only exit_status" do
    CrashMonitor.handle_crash(
      "test-server",
      %Toast.Process.CrashInfo{
        exit_status: 1,
        signal: nil,
        timestamp: :os.system_time(:microsecond)
      }
    )

    assert {:crash, msg} = Abort.reason()
    assert msg =~ "Server crashed"
    assert msg =~ "exit_status=1"
    refute msg =~ "signal"
  end

  test "handle_crash kills the registered test pid" do
    # Spawn a process that just blocks
    victim =
      spawn(fn ->
        receive do
          :never -> :ok
        end
      end)

    ref = Process.monitor(victim)
    Abort.register_test_pid(victim)

    CrashMonitor.handle_crash(
      "test-server",
      %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: :os.system_time(:microsecond)
      }
    )

    assert_receive {:DOWN, ^ref, :process, ^victim, :killed}, 1000
  end
end

defmodule ToastTest.CrashMonitorRescueTest do
  use ExUnit.Case, async: false

  test "silently handles missing Abort ETS table" do
    # Do NOT set up the Abort ETS table — the rescue clause should catch the ArgumentError
    result =
      ToastTest.CrashMonitor.handle_crash(
        "test",
        %Toast.Process.CrashInfo{
          signal: 11,
          exit_status: 139,
          timestamp: :os.system_time(:microsecond)
        }
      )

    assert result == :ok
  end
end

defmodule ToastTest.AbortTestPidTest do
  use ExUnit.Case, async: false

  alias ToastTest.Abort

  setup do
    Abort.clear!()
    on_exit(fn -> Abort.clear!() end)
    :ok
  end

  test "register_test_pid and kill_test_pid kills the registered process" do
    victim =
      spawn(fn ->
        receive do
          :never -> :ok
        end
      end)

    ref = Process.monitor(victim)
    Abort.register_test_pid(victim)
    Abort.kill_test_pid()

    assert_receive {:DOWN, ^ref, :process, ^victim, :killed}, 1000
  end

  test "unregister_test_pid prevents kill" do
    victim =
      spawn(fn ->
        receive do
          :never -> :ok
        end
      end)

    ref = Process.monitor(victim)
    Abort.register_test_pid(victim)
    Abort.unregister_test_pid()
    Abort.kill_test_pid()

    refute_receive {:DOWN, ^ref, :process, ^victim, :killed}, 100
    Process.exit(victim, :kill)
  end
end
