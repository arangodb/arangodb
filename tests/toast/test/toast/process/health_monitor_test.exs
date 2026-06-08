################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule Toast.Process.HealthMonitorTest do
  use ExUnit.Case, async: true

  alias Toast.Process.HealthMonitor

  # Stub health check endpoint - we just test the GenServer behavior
  # by using a non-responsive endpoint and controlling interval/max_failures

  defp monitor_opts(overrides \\ []) do
    Keyword.merge(
      [
        server_id: "test",
        endpoint: "http://127.0.0.1:1",
        listener: self(),
        interval: 60_000
      ],
      overrides
    )
  end

  describe "status/1" do
    test "initial status is :healthy" do
      pid = start_supervised!({HealthMonitor, monitor_opts()})
      assert HealthMonitor.status(pid) == :healthy
    end
  end

  describe "probe_state/1" do
    test "returns :healthy with zero consecutive failures initially" do
      pid = start_supervised!({HealthMonitor, monitor_opts()})
      assert HealthMonitor.probe_state(pid) == :healthy
    end

    test "returns :unhealthy after max_failures" do
      server_id = "probe-unhealthy-test"

      pid =
        start_supervised!(
          {HealthMonitor, monitor_opts(server_id: server_id, interval: 10, max_failures: 1)}
        )

      assert_receive {:server_unhealthy, ^server_id}, 5_000
      assert HealthMonitor.probe_state(pid) == :unhealthy
    end

    test "returns :suspended after suspend/1" do
      pid = start_supervised!({HealthMonitor, monitor_opts()})
      HealthMonitor.suspend(pid)
      assert HealthMonitor.probe_state(pid) == :suspended
    end
  end

  describe "suspend/1" do
    test "changes status to :suspended" do
      pid = start_supervised!({HealthMonitor, monitor_opts()})

      HealthMonitor.suspend(pid)

      assert HealthMonitor.status(pid) == :suspended
    end

    test "is idempotent" do
      pid = start_supervised!({HealthMonitor, monitor_opts()})

      HealthMonitor.suspend(pid)
      HealthMonitor.suspend(pid)
      HealthMonitor.suspend(pid)

      assert HealthMonitor.status(pid) == :suspended
    end

    test "stops health check timer" do
      pid =
        start_supervised!({HealthMonitor, monitor_opts(interval: 10, max_failures: 1)})

      HealthMonitor.suspend(pid)

      Process.sleep(100)

      assert HealthMonitor.status(pid) == :suspended
      refute_received {:server_unhealthy, _}
    end
  end

  describe "unhealthy detection" do
    test "notifies listener after max_failures consecutive failures" do
      server_id = "unhealthy-test"

      pid =
        start_supervised!(
          {HealthMonitor, monitor_opts(server_id: server_id, interval: 10, max_failures: 3)}
        )

      assert_receive {:server_unhealthy, ^server_id}, 5_000
      assert HealthMonitor.status(pid) == :unhealthy
    end

    test "stops polling after becoming unhealthy" do
      server_id = "unhealthy-stop-test"

      pid =
        start_supervised!(
          {HealthMonitor, monitor_opts(server_id: server_id, interval: 10, max_failures: 1)}
        )

      assert_receive {:server_unhealthy, ^server_id}, 5_000

      Process.sleep(100)
      refute_received {:server_unhealthy, _}

      assert HealthMonitor.status(pid) == :unhealthy
    end
  end

  describe "resume/1" do
    test "restores monitoring after suspend" do
      pid = start_supervised!({HealthMonitor, monitor_opts()})

      HealthMonitor.suspend(pid)

      assert HealthMonitor.status(pid) == :suspended

      HealthMonitor.resume(pid)

      assert HealthMonitor.status(pid) == :healthy
    end

    test "after multiple suspends, single resume restores monitoring" do
      pid = start_supervised!({HealthMonitor, monitor_opts()})

      HealthMonitor.suspend(pid)
      HealthMonitor.suspend(pid)
      HealthMonitor.suspend(pid)

      assert HealthMonitor.status(pid) == :suspended

      HealthMonitor.resume(pid)

      assert HealthMonitor.status(pid) == :healthy
    end

    test "on non-suspended monitor is a no-op" do
      pid = start_supervised!({HealthMonitor, monitor_opts()})

      HealthMonitor.resume(pid)

      assert HealthMonitor.status(pid) == :healthy
    end
  end
end
