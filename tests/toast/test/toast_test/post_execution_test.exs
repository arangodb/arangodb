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

defmodule ToastTest.Runner.PostExecutionTest do
  @moduledoc """
  Per-stage degradation (F8): a diagnostics system must produce partial output.
  Event-derived issues (test failures, crashes recorded as events) must survive
  even when the filesystem-enrichment stages have nothing to read.
  """
  use ExUnit.Case, async: false

  import ExUnit.CaptureLog, only: [with_log: 1]

  alias Toast.Process.CrashInfo
  alias ToastTest.{Config, EventStore}
  alias ToastTest.Runner.PostExecution

  @started ~U[2026-03-09 10:00:00Z]
  @finished ~U[2026-03-09 10:05:00Z]

  setup do
    EventStore.clear()

    result_dir =
      Path.join(System.tmp_dir!(), "toast_post_exec_test_#{System.unique_integer([:positive])}")

    on_exit(fn -> File.rm_rf!(result_dir) end)
    {:ok, result_dir: result_dir}
  end

  defp test_config(result_dir) do
    Config.new(result_dir: result_dir, coredump_dir: nil, debugger: :none)
  end

  defp test_data(failures) do
    %{
      suite: "post_exec_test",
      started_at: @started,
      finished_at: @finished,
      times_us: %{async: 0, load: 0, run: 1_000},
      modules: %{},
      failures: failures
    }
  end

  defp failing_test(module, name) do
    %ExUnit.Test{
      name: name,
      module: module,
      state: {:failed, []},
      tags: %{file: "t.exs", line: 1}
    }
  end

  test "event-derived crash and test-failure issues survive with no filesystem artifacts", %{
    result_dir: result_dir
  } do
    # An unexpected crash recorded purely as an event — no server_dir, no log,
    # no coredump on disk for the enrichment stages to find.
    EventStore.notify(%{
      event: :server_crashed,
      deployment_id: "d1",
      server_id: "single1",
      pid: 4242,
      expected: false,
      crash_info: %CrashInfo{
        exit_status: 139,
        signal: 11,
        os_pid: 4242,
        executable: "/usr/bin/arangod",
        timestamp: 1_773_050_430_000_000
      },
      timestamp: 1_773_050_430_000_000
    })

    data = test_data([failing_test(SomeMod, :test_one)])

    {result, _log} = with_log(fn -> PostExecution.run(data, test_config(result_dir)) end)

    types = Enum.map(result.issues, & &1.type)
    assert :crash in types
    assert :test_failure in types

    # A real, fully-assembled result — not the all-or-nothing degraded fallback.
    assert result.suite == "post_exec_test"
    assert File.exists?(Path.join(result_dir, "outcomes.json"))
  end
end
