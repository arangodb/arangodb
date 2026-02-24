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

  describe "healthy?/1" do
    test "returns true when healthy" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      assert HealthMonitor.healthy?(pid) == true
      HealthMonitor.stop(pid)
    end
  end

  describe "suspend/resume" do
    test "suspend changes status to :suspended" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      send(pid, :suspend)
      :sys.get_state(pid)

      assert HealthMonitor.status(pid) == :suspended
      assert HealthMonitor.healthy?(pid) == false
      HealthMonitor.stop(pid)
    end

    test "resume restores monitoring after suspend" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      send(pid, :suspend)
      :sys.get_state(pid)
      assert HealthMonitor.status(pid) == :suspended

      send(pid, :resume)
      :sys.get_state(pid)
      assert HealthMonitor.status(pid) == :healthy

      HealthMonitor.stop(pid)
    end

    test "multiple suspends then single resume restores monitoring" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      send(pid, :suspend)
      send(pid, :suspend)
      send(pid, :suspend)
      :sys.get_state(pid)
      assert HealthMonitor.status(pid) == :suspended

      send(pid, :resume)
      :sys.get_state(pid)
      assert HealthMonitor.status(pid) == :healthy

      HealthMonitor.stop(pid)
    end

    test "resume when not suspended is a no-op" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 60_000
        )

      send(pid, :resume)
      :sys.get_state(pid)
      assert HealthMonitor.status(pid) == :healthy

      HealthMonitor.stop(pid)
    end

    test "suspended monitor does not fire :check messages" do
      {:ok, pid} =
        HealthMonitor.start_link(
          server_id: "test",
          endpoint: "http://127.0.0.1:1",
          listener: self(),
          interval: 10,
          max_failures: 1
        )

      send(pid, :suspend)
      :sys.get_state(pid)

      # Wait enough time for checks to fire if they were going to
      Process.sleep(100)

      # Should still be suspended, not unhealthy
      assert HealthMonitor.status(pid) == :suspended
      refute_received {:server_unhealthy, _}

      HealthMonitor.stop(pid)
    end
  end
end
