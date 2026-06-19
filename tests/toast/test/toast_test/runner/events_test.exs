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

defmodule ToastTest.Runner.EventsTest do
  use ExUnit.Case, async: false

  alias ToastTest.{EventStore, ExUnitCompat, Runner}

  setup do
    EventStore.clear()
    on_exit(fn -> EventStore.clear() end)
    :ok
  end

  defp recorded do
    [event] = EventStore.events()
    {ts, event} = Map.pop!(event, :timestamp)
    assert is_integer(ts)
    event
  end

  defp test_struct(fields) do
    %ExUnit.Test{
      module: SomeMod,
      name: :"a test",
      tags: %{},
      state: Keyword.get(fields, :state),
      time: Keyword.get(fields, :time, 0)
    }
  end

  describe "test lifecycle events" do
    setup do
      # The lifecycle constructors broadcast to ExUnit via the event manager
      # in addition to recording in the EventStore.
      {:ok, manager} = ExUnitCompat.start_event_manager()
      %{manager: manager}
    end

    test "suite_started/2 records a suite_started event", %{manager: manager} do
      Runner.Events.suite_started(manager, [])

      assert recorded() == %{event: :suite_started}
    end

    test "suite_finished/2 records a suite_finished event", %{manager: manager} do
      Runner.Events.suite_finished(manager, %{async: 0, load: nil, run: 1000})

      assert recorded() == %{event: :suite_finished}
    end

    test "module_started/4 records the module", %{manager: manager} do
      Runner.Events.module_started(manager, SomeMod, %ExUnit.TestModule{name: SomeMod, state: nil})

      assert recorded() == %{event: :module_started, module: SomeMod}
    end

    test "module_finished/4 records the module", %{manager: manager} do
      Runner.Events.module_finished(manager, SomeMod, %ExUnit.TestModule{
        name: SomeMod,
        state: nil
      })

      assert recorded() == %{event: :module_finished, module: SomeMod}
    end

    test "test_started/3 records module and name", %{manager: manager} do
      Runner.Events.test_started(manager, test_struct(state: nil))

      assert recorded() == %{event: :test_started, module: SomeMod, name: :"a test"}
    end

    test "test_finished/3 projects outcome and duration from the test struct",
         %{manager: manager} do
      Runner.Events.test_finished(manager, test_struct(state: nil, time: 1234))

      assert recorded() == %{
               event: :test_finished,
               module: SomeMod,
               name: :"a test",
               outcome: :passed,
               duration_us: 1234
             }
    end

    test "test_finished/3 maps a failed test to outcome :failed", %{manager: manager} do
      Runner.Events.test_finished(manager, test_struct(state: {:failed, []}, time: 99))

      assert %{event: :test_finished, outcome: :failed, duration_us: 99} = recorded()
    end

    test "an explicit :timestamp option is preserved", %{manager: manager} do
      Runner.Events.test_started(manager, test_struct(state: nil), timestamp: 123)

      [event] = EventStore.events()
      assert event.timestamp == 123
    end
  end

  describe "between_tests_finished/2" do
    test "records the barrier end for a test" do
      Runner.Events.between_tests_finished(SomeMod, :"test one")

      assert recorded() == %{
               event: :between_tests_finished,
               module: SomeMod,
               name: :"test one"
             }
    end
  end

  describe "diagnostics events" do
    test "netstat_snapshot/2 with a label" do
      Runner.Events.netstat_snapshot(1500, :pre_deployment)

      assert recorded() == %{event: :netstat_snapshot, total: 1500, label: :pre_deployment}
    end

    test "netstat_snapshot/1 defaults label to nil" do
      Runner.Events.netstat_snapshot(1500)

      assert recorded() == %{event: :netstat_snapshot, total: 1500, label: nil}
    end

    test "infrastructure_issue/2" do
      detail = %{kind: :system, total: 16_000, threshold: 15_000}

      Runner.Events.infrastructure_issue(:port_exhaustion, detail)

      assert recorded() == %{
               event: :infrastructure_issue,
               subtype: :port_exhaustion,
               detail: detail
             }
    end

    test "timeout_kill/3" do
      servers = [%{server_id: "single1", os_pid: 4242, log_file: "/data/single1/arangod.log"}]

      Runner.Events.timeout_kill(:global, "Global execution timeout exceeded", servers)

      assert recorded() == %{
               event: :timeout_kill,
               source: :global,
               reason: "Global execution timeout exceeded",
               servers: servers
             }
    end
  end
end
