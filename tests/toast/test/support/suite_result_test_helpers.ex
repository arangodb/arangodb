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

defmodule ToastTest.SuiteResultTestHelpers do
  @moduledoc false

  @suite_started_at ~U[2026-03-09 10:00:00Z]
  @suite_finished_at ~U[2026-03-09 10:05:00Z]

  @mod_started_at ~U[2026-03-09 10:00:01Z]
  @mod_finished_at ~U[2026-03-09 10:04:59Z]
  @setup_finished_at ~U[2026-03-09 10:00:02Z]
  @teardown_started_at ~U[2026-03-09 10:04:58Z]

  @test1_started_at ~U[2026-03-09 10:00:02Z]
  @test1_finished_at ~U[2026-03-09 10:01:00Z]
  @test2_started_at ~U[2026-03-09 10:01:01Z]
  @test2_finished_at ~U[2026-03-09 10:02:00Z]

  # All timestamps are microseconds since the epoch — the unified representation
  # used by the event stream, TimeWindows, and SuiteResult.
  def suite_started_at, do: us(@suite_started_at)
  def suite_finished_at, do: us(@suite_finished_at)
  def mod_started_at, do: us(@mod_started_at)
  def mod_finished_at, do: us(@mod_finished_at)
  def setup_finished_at, do: us(@setup_finished_at)
  def teardown_started_at, do: us(@teardown_started_at)
  def test1_started_at, do: us(@test1_started_at)
  def test1_finished_at, do: us(@test1_finished_at)
  def test2_started_at, do: us(@test2_started_at)
  def test2_finished_at, do: us(@test2_finished_at)

  defp us(dt), do: DateTime.to_unix(dt, :microsecond)

  # Collector-shape test_data: outcomes/durations/tags only. Module and per-test
  # time windows live in `build_windows/0` and are attached by `SuiteResult.build`.
  def build_test_data(overrides \\ %{}) do
    defaults = %{
      suite: "smoke",
      times_us: %{async: 0, load: 5000, run: 300_000_000},
      modules: %{
        FakeModule => %{
          tests: [
            %{
              name: :"test passes",
              outcome: :passed,
              duration_us: 58_000_000,
              tags: %{file: "test/fake_test.exs", line: 10}
            },
            %{
              name: :"test fails",
              outcome: :failed,
              duration_us: 59_000_000,
              tags: %{file: "test/fake_test.exs", line: 20}
            }
          ]
        }
      }
    }

    Map.merge(defaults, overrides)
  end

  # Event-derived windows matching the default `build_test_data/1` modules.
  def build_windows do
    %{
      suite: %{started_at: suite_started_at(), finished_at: suite_finished_at()},
      modules: %{
        FakeModule => %{
          started_at: mod_started_at(),
          finished_at: mod_finished_at(),
          setup_finished_at: setup_finished_at(),
          teardown_started_at: teardown_started_at()
        }
      },
      tests: %{
        {FakeModule, :"test passes"} => %{
          started_at: test1_started_at(),
          finished_at: test1_finished_at()
        },
        {FakeModule, :"test fails"} => %{
          started_at: test2_started_at(),
          finished_at: test2_finished_at()
        }
      }
    }
  end

  def build_issues do
    [
      %{
        type: :test_failure,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{test: %{name: :"test fails", module: FakeModule}}
      },
      %{
        type: :crash,
        scope: {:module, FakeModule},
        confidence: :low,
        detail: %{
          server: "srv-1",
          coredumps: [%{core_path: "/tmp/core.1234", signal: "SIGABRT", threads: []}],
          logs: "some log output"
        }
      }
    ]
  end

  def build_sanitizer_issue do
    %{
      type: :sanitizer_report,
      scope: :suite,
      confidence: nil,
      detail: %{server: "srv-1", report: "ASAN detected leak"}
    }
  end

  def build_suite_result(opts \\ []) do
    test_data = Keyword.get(opts, :test_data, build_test_data())
    issues = Keyword.get(opts, :issues, build_issues())
    events = Keyword.get(opts, :events, [])
    deployments = Keyword.get(opts, :deployments, %{})

    ToastTest.SuiteResult.build(test_data, issues,
      windows: Keyword.get(opts, :windows, build_windows()),
      events: events,
      deployments: deployments
    )
  end

  def with_tmp_dir(prefix \\ "suite_result_test", fun) do
    dir = Path.join(System.tmp_dir!(), "#{prefix}_#{:erlang.unique_integer([:positive])}")
    File.mkdir_p!(dir)

    try do
      fun.(dir)
    after
      File.rm_rf!(dir)
    end
  end
end
