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

defmodule Mix.Tasks.Toast.Analyze.Detail.BodyTest do
  use ExUnit.Case, async: false

  alias Mix.Tasks.Toast.Analyze.Detail.Body

  @no_color %{colors_enabled: false}

  setup do
    Mix.shell(Mix.Shell.Process)
    on_exit(fn -> Mix.shell(Mix.Shell.IO) end)
    :ok
  end

  defp render(issue, bt_opts \\ %{}) do
    Body.print(issue, @no_color, bt_opts)
    collect_output()
  end

  defp collect_output(acc \\ []) do
    receive do
      {:mix_shell, :info, [msg]} -> collect_output([msg | acc])
    after
      0 -> acc |> Enum.reverse() |> Enum.join("\n")
    end
  end

  # --- crash "at:" line (print_crash_info) ---
  #
  # crash_info.timestamp is an integer-µs value. The rendered crash summary
  # should include an `at: <ISO8601>` segment derived from it.

  describe "crash issue — crash_info timestamp" do
    test "renders the crash time from a µs-integer timestamp" do
      ts = DateTime.to_unix(~U[2026-03-09 10:00:00Z], :microsecond)

      issue = %{
        type: :crash,
        detail: %{
          crash_info: %{os_pid: 1234, signal: nil, exit_status: 139, timestamp: ts}
        }
      }

      output = render(issue)

      assert output =~ "PID 1234"
      assert output =~ "exit_status: 139"
      assert output =~ "at: 2026-03-09T10:00:00.000000Z"
    end

    test "omits the at: segment when timestamp is nil" do
      issue = %{
        type: :crash,
        detail: %{
          crash_info: %{os_pid: 1234, signal: nil, exit_status: 139, timestamp: nil}
        }
      }

      output = render(issue)

      assert output =~ "PID 1234"
      refute output =~ "at:"
    end
  end

  # --- netstat trajectory test timeline (build_test_timeline) ---
  #
  # For a port_exhaustion issue, each netstat snapshot is labelled with the most
  # recently finished test (by µs `finished_at`). With µs-integer windows the
  # timeline must associate each snapshot with the correct test name.

  describe "port_exhaustion issue — netstat trajectory test labels" do
    test "labels each snapshot with the most recently finished test" do
      t_alpha = 1000
      t_beta = 2000
      issue_ts = 3000

      modules = %{
        MyMod => %{
          tests: [
            %{name: :"test alpha", finished_at: t_alpha},
            %{name: :"test beta", finished_at: t_beta}
          ]
        }
      }

      issue = %{
        type: :infrastructure,
        detail: %{
          subtype: :port_exhaustion,
          timestamp: issue_ts,
          total: 100,
          threshold: 50,
          kind: :system,
          by_server: %{}
        },
        modules: modules,
        events: [
          # after test alpha finished (1000), before beta
          %{event: :netstat_snapshot, total: 10, timestamp: 1500, label: nil},
          # after test beta finished (2000); coincides with the threshold
          %{event: :netstat_snapshot, total: 100, timestamp: issue_ts, label: nil}
        ]
      }

      output = render(issue)

      assert output =~ "MyMod.test alpha"
      assert output =~ "MyMod.test beta"
      # The snapshot at the issue timestamp is the threshold marker.
      assert output =~ ~r/MyMod\.test beta.*THRESHOLD/
    end

    test "labels snapshots before any test finishes with a placeholder" do
      modules = %{
        MyMod => %{tests: [%{name: :"test alpha", finished_at: 5000}]}
      }

      issue = %{
        type: :infrastructure,
        detail: %{
          subtype: :port_exhaustion,
          timestamp: 1000,
          total: 100,
          threshold: 50,
          kind: :system,
          by_server: %{}
        },
        modules: modules,
        events: [
          %{event: :netstat_snapshot, total: 100, timestamp: 1000, label: nil}
        ]
      }

      output = render(issue)

      # The only snapshot precedes the test's finished_at -> no test label.
      refute output =~ "MyMod.test alpha"
      assert output =~ ~r/100.*THRESHOLD/
    end
  end
end
