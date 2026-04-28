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

defmodule Toast.Deployment.ControllerAwaitCrashTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.{Config, Controller, ServerInstance}
  alias Toast.Process.CrashInfo

  defp start_controller(operational_state \\ :running) do
    id = "await-crash-test-#{System.unique_integer([:positive])}"

    server = %ServerInstance{
      id: id,
      role: :single,
      operational_state: operational_state,
      expecting_exit: false
    }

    {:ok, ctrl} =
      Controller.start_link(
        config: Config.new(),
        id: id,
        servers: %{id => server},
        status: :ready
      )

    on_exit(fn ->
      try do
        GenServer.stop(ctrl)
      catch
        :exit, _ -> :ok
      end
    end)

    {ctrl, id}
  end

  defp crash_info(overrides \\ []) do
    defaults = [exit_status: 139, signal: 11, timestamp: :os.system_time(:microsecond)]
    struct!(CrashInfo, Keyword.merge(defaults, overrides))
  end

  describe "await_crash_event/3" do
    test "returns :ok immediately if server is already in :crashed state" do
      {ctrl, server_id} = start_controller(:crashed)

      assert Controller.await_crash_event(ctrl, server_id, 1_000) == :ok
    end

    test "blocks until an unexpected crash event is processed" do
      {ctrl, server_id} = start_controller()

      task = Task.async(fn -> Controller.await_crash_event(ctrl, server_id, 5_000) end)

      # Give the call time to register as a waiter.
      Process.sleep(50)
      refute Task.yield(task, 0)

      Controller.notify_crash(ctrl, server_id, crash_info())

      assert Task.await(task, 1_000) == :ok
    end

    test "blocks until an expected crash event is processed" do
      {ctrl, server_id} = start_controller()
      :ok = Controller.expect_crash(ctrl, server_id, 5_000)

      task = Task.async(fn -> Controller.await_crash_event(ctrl, server_id, 5_000) end)

      Process.sleep(50)
      refute Task.yield(task, 0)

      Controller.notify_crash(ctrl, server_id, crash_info())

      assert Task.await(task, 1_000) == :ok
    end

    test "returns :timeout if no crash event arrives in time" do
      {ctrl, server_id} = start_controller()

      assert Controller.await_crash_event(ctrl, server_id, 100) == :timeout
    end

    test "multiple concurrent waiters for the same server are all released" do
      {ctrl, server_id} = start_controller()

      tasks =
        for _ <- 1..3 do
          Task.async(fn -> Controller.await_crash_event(ctrl, server_id, 5_000) end)
        end

      Process.sleep(50)

      Controller.notify_crash(ctrl, server_id, crash_info())

      for task <- tasks do
        assert Task.await(task, 1_000) == :ok
      end
    end

    test "returns :timeout if a different server crashes" do
      {ctrl, server_id} = start_controller()

      # Call with a server_id that doesn't exist — should still park and
      # time out rather than releasing on an unrelated crash.
      task =
        Task.async(fn ->
          Controller.await_crash_event(ctrl, "other-server", 200)
        end)

      Controller.notify_crash(ctrl, server_id, crash_info())

      assert Task.await(task, 1_000) == :timeout
    end
  end
end
