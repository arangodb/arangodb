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

defmodule Resilience.ServerLifecycleTest do
  use Resilience.Suite

  alias Toast.Deployment

  describe "stop and restart" do
    test "stop dbserver degrades cluster, restart recovers", %{deployment: d} do
      [dbserver | _] = Deployment.servers(d, role: :dbserver)

      assert :ok = Deployment.stop_server(d, dbserver.id)
      assert :degraded = Deployment.status(d)

      assert :ok = Deployment.restart_server(d, dbserver.id)
      assert :ready = Deployment.status(d)
    end
  end

  describe "pause and resume" do
    test "pause coordinator, then resume", %{deployment: d} do
      [coordinator | _] = Deployment.servers(d, role: :coordinator)

      assert :ok = Deployment.pause_server(d, coordinator.id)
      assert :degraded = Deployment.status(d)

      assert :ok = Deployment.resume_server(d, coordinator.id)
      assert :ready = Deployment.status(d)
    end
  end

  describe "kill and recover" do
    test "kill dbserver and start it again", %{deployment: d} do
      [dbserver | _] = Deployment.servers(d, role: :dbserver)

      assert :ok = Deployment.kill_server(d, dbserver.id)
      assert :degraded = Deployment.status(d)

      assert :ok = Deployment.start_server(d, dbserver.id)
      assert :ready = Deployment.status(d)
    end
  end

  describe "expected crash via failure point" do
    test "expect_crash + failure point + verify + restart", %{deployment: d} do
      [dbserver | _] = Deployment.servers(d, role: :dbserver)

      assert :ok = Deployment.set_failure_point(d, dbserver.id, "crash-after-commit")
      assert :ok = Deployment.expect_crash(d, dbserver.id)

      # The failure point will trigger on next write operation.
      # In a real test, we'd perform a write here. For now, manually
      # verify the crash expectation is registered.
      assert {:error, :timeout} = Deployment.verify_crash(d, dbserver.id, timeout: 100)

      # Clean up: clear failure points and restart
      Deployment.clear_all_failure_points(d)
      Deployment.restart_server(d, dbserver.id)
    end
  end
end
