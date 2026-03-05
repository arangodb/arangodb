defmodule ToastTest.CrashMonitorTest do
  use ExUnit.Case, async: false

  alias ToastTest.{CrashMonitor, Runner}

  setup do
    Runner.clear_abort!()
    on_exit(fn -> Runner.clear_abort!() end)
    :ok
  end

  defp dummy_deployment do
    %Toast.Deployment{
      id: "test",
      mode: :single_server,
      config: Toast.Config.load(),
      controller: self(),
      endpoint: "http://127.0.0.1:0",
      work_dir: "/tmp/toast-test"
    }
  end

  test "handle_crash sets abort with signal and exit_status" do
    CrashMonitor.handle_crash(
      dummy_deployment(),
      %Toast.Process.CrashInfo{signal: 11, exit_status: 139, timestamp: DateTime.utc_now()}
    )

    assert {:crash, msg} = Runner.aborted?()
    assert msg =~ "Server crashed"
    assert msg =~ "signal: 11"
    assert msg =~ "exit_status=139"
  end

  test "handle_crash with only exit_status" do
    CrashMonitor.handle_crash(
      dummy_deployment(),
      %Toast.Process.CrashInfo{exit_status: 1, signal: nil, timestamp: DateTime.utc_now()}
    )

    assert {:crash, msg} = Runner.aborted?()
    assert msg =~ "Server crashed"
    assert msg =~ "exit_status=1"
    refute msg =~ "signal"
  end

  test "handle_crash with nil signal and exit_status" do
    CrashMonitor.handle_crash(
      dummy_deployment(),
      %Toast.Process.CrashInfo{exit_status: 134, signal: 6, timestamp: DateTime.utc_now()}
    )

    assert {:crash, msg} = Runner.aborted?()
    assert msg =~ "Server crashed"
  end

  test "handle_crash returns :ok" do
    assert :ok =
             CrashMonitor.handle_crash(
               dummy_deployment(),
               %Toast.Process.CrashInfo{
                 exit_status: nil,
                 signal: nil,
                 timestamp: DateTime.utc_now()
               }
             )
  end

  test "callback has arity 2" do
    assert is_function(&CrashMonitor.handle_crash/2, 2)
  end
end
