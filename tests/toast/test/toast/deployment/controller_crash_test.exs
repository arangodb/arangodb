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

defmodule Toast.Deployment.ControllerCrashTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.{Config, Controller, ServerInstance}
  alias Toast.Process.CrashInfo

  defp start_controller(server_overrides \\ []) do
    id = "crash-test-#{System.unique_integer([:positive])}"

    defaults = [id: id, role: :single, operational_state: :running, expecting_exit: false]
    server = struct!(ServerInstance, Keyword.merge(defaults, server_overrides))

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
    defaults = [
      exit_status: 139,
      signal: 11,
      timestamp: :os.system_time(:microsecond)
    ]

    struct!(CrashInfo, Keyword.merge(defaults, overrides))
  end

  describe "handle_crash classification via :server_crashed message" do
    test "unexpected crash sets status to :failed" do
      {ctrl, server_id} = start_controller()

      Controller.notify_crash(ctrl, server_id, crash_info())

      assert Controller.get_status(ctrl) == :failed
      info = Controller.get_info(ctrl)
      assert {:server_crashed, ^server_id, _} = info.error
      assert info.servers[server_id].operational_state == :crashed
    end

    test "intentional exit (nil signal) does not change status" do
      {ctrl, server_id} = start_controller(expecting_exit: true)

      Controller.notify_crash(ctrl, server_id, crash_info(signal: nil))

      assert Controller.get_status(ctrl) == :ready
      assert Controller.get_info(ctrl).error == nil
    end

    test "intentional exit (SIGTERM signal 15) does not change status" do
      {ctrl, server_id} = start_controller(expecting_exit: true)

      Controller.notify_crash(ctrl, server_id, crash_info(signal: 15))

      assert Controller.get_status(ctrl) == :ready
      assert Controller.get_info(ctrl).error == nil
    end

    test "crash during intentional stop (SIGSEGV) sets status to :failed" do
      {ctrl, server_id} = start_controller(expecting_exit: true)

      Controller.notify_crash(ctrl, server_id, crash_info(signal: 11))

      assert Controller.get_status(ctrl) == :failed
      info = Controller.get_info(ctrl)
      assert {:server_crashed, ^server_id, _} = info.error
    end

    test "crash during intentional stop (SIGABRT signal 6) sets status to :failed" do
      {ctrl, server_id} = start_controller(expecting_exit: true)

      Controller.notify_crash(ctrl, server_id, crash_info(signal: 6))

      assert Controller.get_status(ctrl) == :failed
    end

    test "expected crash (no waiter) stores crash_info and derives status" do
      {ctrl, server_id} = start_controller()

      :ok = Controller.expect_crash(ctrl, server_id, 5_000)

      info = crash_info()
      Controller.notify_crash(ctrl, server_id, info)

      # Sync via get_info — expected crash derives status (degraded, not :failed)
      ctrl_info = Controller.get_info(ctrl)
      assert ctrl_info.servers[server_id].operational_state == :crashed
      assert ctrl_info.status == :degraded
    end

    test "expected crash with pending verify_crash replies to waiter" do
      {ctrl, server_id} = start_controller()

      :ok = Controller.expect_crash(ctrl, server_id, 5_000)

      # Start verify_crash in a task (it will block waiting for crash)
      task =
        Task.async(fn ->
          Controller.verify_crash(ctrl, server_id, 5_000)
        end)

      # Give verify_crash time to register waiter
      Process.sleep(50)

      # Simulate crash
      info = crash_info()
      Controller.notify_crash(ctrl, server_id, info)

      # verify_crash should return the crash info
      assert {:ok, returned_info} = Task.await(task, 5_000)
      assert returned_info.signal == 11
      assert returned_info.exit_status == 139
    end
  end
end
