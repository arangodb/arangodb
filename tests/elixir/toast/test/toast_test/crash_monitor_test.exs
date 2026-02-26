defmodule ToastTest.CrashMonitorTest do
  use ExUnit.Case, async: false

  alias ToastTest.{CrashMonitor, Runner}

  setup do
    Runner.clear_abort!()
    on_exit(fn -> Runner.clear_abort!() end)
    :ok
  end

  test "handle_crash sets abort with server_id and signal" do
    CrashMonitor.handle_crash(%{server_id: "db-1", signal: 11, exit_status: 139})

    assert {:crash, msg} = Runner.aborted?()
    assert msg =~ "Server db-1 crashed"
    assert msg =~ "signal: 11"
    assert msg =~ "exit_status=139"
  end

  test "handle_crash with only exit_status" do
    CrashMonitor.handle_crash(%{server_id: "agent-1", exit_status: 1})

    assert {:crash, msg} = Runner.aborted?()
    assert msg =~ "Server agent-1 crashed"
    assert msg =~ "exit_status=1"
    refute msg =~ "signal"
  end

  test "handle_crash with unknown server" do
    CrashMonitor.handle_crash(%{exit_status: 134, signal: 6})

    assert {:crash, msg} = Runner.aborted?()
    assert msg =~ "Server unknown crashed"
  end

  test "handle_crash returns :ok" do
    assert :ok = CrashMonitor.handle_crash(%{server_id: "x"})
  end

  test "callback has arity 1" do
    assert is_function(&CrashMonitor.handle_crash/1, 1)
  end
end
