defmodule Toast.Process.HealthMonitorTest do
  use ExUnit.Case, async: true

  alias Toast.Process.HealthMonitor

  # Stub health check endpoint - we just test the GenServer behavior
  # by using a non-responsive endpoint and controlling interval/max_failures

  describe "status/1" do
    test "initial status is :healthy" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      assert HealthMonitor.status(pid) == :healthy
      HealthMonitor.stop(pid)
    end
  end

  describe "suspend/1" do
    test "changes status to :suspended" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      HealthMonitor.suspend(pid)

      assert HealthMonitor.status(pid) == :suspended
      assert HealthMonitor.status(pid) != :healthy
      HealthMonitor.stop(pid)
    end

    test "is idempotent" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      HealthMonitor.suspend(pid)
      HealthMonitor.suspend(pid)
      HealthMonitor.suspend(pid)

      assert HealthMonitor.status(pid) == :suspended

      HealthMonitor.stop(pid)
    end

    test "stops health check timer" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 10,
          max_failures: 1
        )

      HealthMonitor.suspend(pid)

      Process.sleep(100)

      assert HealthMonitor.status(pid) == :suspended
      refute_received {:server_unhealthy, _}

      HealthMonitor.stop(pid)
    end
  end

  describe "unhealthy detection" do
    test "notifies listener after max_failures consecutive failures" do
      server_id = "unhealthy-test"

      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: server_id,
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 10,
          max_failures: 3
        )

      assert_receive {:server_unhealthy, ^server_id}, 5_000
      assert HealthMonitor.status(pid) == :unhealthy

      HealthMonitor.stop(pid)
    end

    test "stops polling after becoming unhealthy" do
      server_id = "unhealthy-stop-test"

      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: server_id,
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 10,
          max_failures: 1
        )

      assert_receive {:server_unhealthy, ^server_id}, 5_000

      Process.sleep(100)
      refute_received {:server_unhealthy, _}

      HealthMonitor.stop(pid)
    end
  end

  describe "resume/1" do
    test "restores monitoring after suspend" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      HealthMonitor.suspend(pid)

      assert HealthMonitor.status(pid) == :suspended

      HealthMonitor.resume(pid)

      assert HealthMonitor.status(pid) == :healthy

      HealthMonitor.stop(pid)
    end

    test "after multiple suspends, single resume restores monitoring" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      HealthMonitor.suspend(pid)
      HealthMonitor.suspend(pid)
      HealthMonitor.suspend(pid)

      assert HealthMonitor.status(pid) == :suspended

      HealthMonitor.resume(pid)

      assert HealthMonitor.status(pid) == :healthy

      HealthMonitor.stop(pid)
    end

    test "on non-suspended monitor is a no-op" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      HealthMonitor.resume(pid)

      assert HealthMonitor.status(pid) == :healthy

      HealthMonitor.stop(pid)
    end
  end
end
