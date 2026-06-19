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

defmodule Mix.Tasks.Toast.Analyze.PerfTest do
  use ExUnit.Case, async: false

  import ToastTest.SuiteResultTestHelpers

  alias Mix.Tasks.Toast.Analyze.Perf

  @no_color %{colors_enabled: false}

  setup do
    Mix.shell(Mix.Shell.Process)
    on_exit(fn -> Mix.shell(Mix.Shell.IO) end)
    :ok
  end

  # Writes the given SuiteResult structs to a temp dir as `*.diagnostics.etf`
  # (the on-disk format `Data.load_results/1` consumes) and runs `fun` with the
  # directory. Mirrors the real analyze entry point: load from disk, render.
  defp with_results(results, fun) do
    with_tmp_dir(fn dir ->
      results
      |> Enum.with_index()
      |> Enum.each(fn {result, i} ->
        path = Path.join(dir, "#{i}.diagnostics.etf")
        File.write!(path, :erlang.term_to_binary(result))
      end)

      fun.(dir)
    end)
  end

  defp collect_output do
    collect_output([])
  end

  defp collect_output(acc) do
    receive do
      {:mix_shell, :info, [msg]} -> collect_output([msg | acc])
    after
      0 -> acc |> Enum.reverse() |> Enum.join("\n")
    end
  end

  describe "run/3 — suite/module breakdown (no --module)" do
    test "renders per-module phase durations computed from µs timestamps" do
      with_results([build_suite_result()], fn dir ->
        Perf.run(dir, [], @no_color)
      end)

      output = collect_output()

      # Suite header: run time (300s = 5m0.0s) and total test count.
      assert output =~ "smoke (5m0.0s — 2 tests)"

      # Module row durations are derived from the module window:
      #   total    = finished_at - started_at        = 298s  -> 4m58.0s
      #   setup    = setup_finished_at - started_at   = 1s    -> 1.0s
      #   teardown = finished_at - teardown_started_at = 1s    -> 1.0s
      # The row also reports the test count (2).
      assert output =~ "FakeModule"
      assert output =~ "4m58.0s"
      assert output =~ ~r/FakeModule\s+4m58\.0s\s+2\s+1\.0s\s+1\.0s/
    end

    test "module with no tests still renders with zero test count" do
      test_data = build_test_data(%{modules: %{FakeModule => %{tests: []}}})

      windows = %{
        modules: %{
          FakeModule => %{
            started_at: mod_started_at(),
            finished_at: mod_finished_at(),
            setup_finished_at: setup_finished_at(),
            teardown_started_at: teardown_started_at()
          }
        },
        tests: %{}
      }

      result = build_suite_result(test_data: test_data, windows: windows, issues: [])

      with_results([result], fn dir -> Perf.run(dir, [], @no_color) end)

      output = collect_output()

      assert output =~ "smoke (5m0.0s — 0 tests)"
      assert output =~ ~r/FakeModule\s+4m58\.0s\s+0\s/
    end

    test "module with nil setup/teardown windows reports zero setup and teardown" do
      test_data =
        build_test_data(%{
          modules: %{
            FakeModule => %{
              tests: [%{name: :"test a", outcome: :passed, duration_us: 1_000_000, tags: %{}}]
            }
          }
        })

      windows = %{
        modules: %{
          FakeModule => %{
            started_at: mod_started_at(),
            finished_at: mod_finished_at(),
            setup_finished_at: nil,
            teardown_started_at: nil
          }
        },
        tests: %{}
      }

      result = build_suite_result(test_data: test_data, windows: windows, issues: [])

      with_results([result], fn dir -> Perf.run(dir, [], @no_color) end)

      output = collect_output()

      # No setup/teardown windows -> both phases are 0s, so tests_us == total.
      assert output =~ ~r/FakeModule\s+4m58\.0s\s+1\s+0µs\s+0µs/
    end

    test "suite with no modules renders only the legend, no suite header" do
      result =
        build_suite_result(
          test_data: build_test_data(%{modules: %{}}),
          windows: %{modules: %{}, tests: %{}},
          issues: []
        )

      with_results([result], fn dir -> Perf.run(dir, [], @no_color) end)

      output = collect_output()

      assert output =~ "Legend:"
      refute output =~ "smoke ("
    end
  end

  describe "run/3 — per-test breakdown (--module)" do
    test "renders per-test durations and outcomes for the matched module" do
      with_results([build_suite_result()], fn dir ->
        Perf.run(dir, [module: "FakeModule"], @no_color)
      end)

      output = collect_output()

      # Module header reports total/setup/teardown derived from the µs window.
      assert output =~ "FakeModule (smoke) — 4m58.0s total, setup 1.0s, teardown 1.0s"

      # Tests are sorted by duration descending: "fails" (59s) before "passes" (58s).
      assert output =~ ~r/fails\s+59\.0s\s+failed/
      assert output =~ ~r/passes\s+58\.0s\s+passed/

      fails_idx = :binary.match(output, "fails") |> elem(0)
      passes_idx = :binary.match(output, "passes") |> elem(0)
      assert fails_idx < passes_idx
    end

    test "no module match raises" do
      assert_raise Mix.Error, ~r/No module matching/, fn ->
        with_results([build_suite_result()], fn dir ->
          Perf.run(dir, [module: "NoSuchModule"], @no_color)
        end)
      end
    end
  end
end
