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

    reason = Runner.aborted?()
    assert reason =~ "Server db-1 crashed"
    assert reason =~ "signal: 11"
    assert reason =~ "exit_status=139"
  end

  test "handle_crash with only exit_status" do
    CrashMonitor.handle_crash(%{server_id: "agent-1", exit_status: 1})

    reason = Runner.aborted?()
    assert reason =~ "Server agent-1 crashed"
    assert reason =~ "exit_status=1"
    refute reason =~ "signal"
  end

  test "handle_crash with unknown server" do
    CrashMonitor.handle_crash(%{exit_status: 134, signal: 6})

    reason = Runner.aborted?()
    assert reason =~ "Server unknown crashed"
  end

  test "handle_crash returns :ok" do
    assert :ok = CrashMonitor.handle_crash(%{server_id: "x"})
  end

  test "callback has arity 1" do
    assert is_function(&CrashMonitor.handle_crash/1, 1)
  end
end
