defmodule Toast.Process.HealthMonitorSuspendTest do
  use ExUnit.Case, async: true

  alias Toast.Process.HealthMonitor

  setup do
    {:ok, pid} =
      HealthMonitor.start_link(
        server_id: "test-hm-#{System.unique_integer([:positive])}",
        endpoint: "http://127.0.0.1:1",
        listener: self(),
        interval: 50_000
      )

    on_exit(fn -> if Process.alive?(pid), do: HealthMonitor.stop(pid) end)
    %{pid: pid}
  end

  test "starts healthy", %{pid: pid} do
    assert HealthMonitor.status(pid) == :healthy
  end

  test "suspend transitions to :suspended", %{pid: pid} do
    HealthMonitor.suspend(pid)
    Process.sleep(50)
    assert HealthMonitor.status(pid) == :suspended
  end

  test "resume after suspend transitions back to :healthy", %{pid: pid} do
    HealthMonitor.suspend(pid)
    Process.sleep(50)
    HealthMonitor.resume(pid)
    Process.sleep(50)
    assert HealthMonitor.status(pid) == :healthy
  end

  test "resume on non-suspended is a no-op", %{pid: pid} do
    HealthMonitor.resume(pid)
    Process.sleep(50)
    assert HealthMonitor.status(pid) == :healthy
  end

  test "suspend is idempotent", %{pid: pid} do
    HealthMonitor.suspend(pid)
    HealthMonitor.suspend(pid)
    Process.sleep(50)
    assert HealthMonitor.status(pid) == :suspended
  end
end
