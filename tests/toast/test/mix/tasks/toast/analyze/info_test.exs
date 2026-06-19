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

defmodule Mix.Tasks.Toast.Analyze.InfoTest do
  use ExUnit.Case, async: false

  import ToastTest.SuiteResultTestHelpers

  alias Mix.Tasks.Toast.Analyze.Info

  @no_color %{colors_enabled: false}

  setup do
    Mix.shell(Mix.Shell.Process)
    on_exit(fn -> Mix.shell(Mix.Shell.IO) end)
    :ok
  end

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

  defp collect_output(acc \\ []) do
    receive do
      {:mix_shell, :info, [msg]} -> collect_output([msg | acc])
    after
      0 -> acc |> Enum.reverse() |> Enum.join("\n")
    end
  end

  describe "run/3 — suite summary" do
    test "renders suite name, µs time range as ISO8601, and runtime" do
      with_results([build_suite_result()], fn dir -> Info.run(dir, [], @no_color) end)

      output = collect_output()

      assert output =~ "Suite: smoke"
      # started_at/finished_at are µs integers; Data.fmt_dt renders them as ISO8601.
      assert output =~ "Time:    2026-03-09T10:00:00.000000Z .. 2026-03-09T10:05:00.000000Z"
      # times_us.run = 300_000_000µs -> 300.0s.
      assert output =~ "Runtime: 300.0s"
    end

    test "renders module count and per-outcome test tallies" do
      with_results([build_suite_result()], fn dir -> Info.run(dir, [], @no_color) end)

      output = collect_output()

      # build_test_data has one module with one passed and one failed test.
      assert output =~ "Modules: 1  Tests: 2 (1 passed, 1 failed, 0 skipped)"
    end

    test "renders issue count with per-type frequencies" do
      with_results([build_suite_result()], fn dir -> Info.run(dir, [], @no_color) end)

      output = collect_output()

      # build_issues has one test_failure and one crash.
      assert output =~ ~r/Issues:  2 \(.*1 test_failure.*\)/
      assert output =~ "1 crash"
    end

    test "renders zero issues without a frequency breakdown" do
      result = build_suite_result(issues: [])

      with_results([result], fn dir -> Info.run(dir, [], @no_color) end)

      output = collect_output()

      assert output =~ "Issues:  0"
    end

    test "renders a missing finished_at as a question mark" do
      result = %{build_suite_result(issues: []) | finished_at: nil}

      with_results([result], fn dir -> Info.run(dir, [], @no_color) end)

      output = collect_output()

      assert output =~ "Time:    2026-03-09T10:00:00.000000Z .. ?"
    end

    test "counts invalidated tests separately and includes them only when present" do
      test_data =
        build_test_data(%{
          modules: %{
            FakeModule => %{
              tests: [
                %{name: :"test ok", outcome: :passed, duration_us: 1, tags: %{}},
                %{name: :"test gone", outcome: :invalidated, duration_us: 1, tags: %{}}
              ]
            }
          }
        })

      result =
        build_suite_result(test_data: test_data, windows: %{modules: %{}, tests: %{}}, issues: [])

      with_results([result], fn dir -> Info.run(dir, [], @no_color) end)

      output = collect_output()

      assert output =~ "Tests: 2 (1 passed, 0 failed, 0 skipped, 1 invalidated)"
    end

    test "renders deployments count of zero for a suite with no deployments" do
      result = build_suite_result(issues: [])

      with_results([result], fn dir -> Info.run(dir, [], @no_color) end)

      output = collect_output()

      assert output =~ "Deployments (0):"
    end
  end

  describe "run/3 — suite filtering" do
    test "renders only the matching suite when --suite is given" do
      other = %{build_suite_result(issues: []) | suite: "stress"}
      smoke = build_suite_result(issues: [])

      with_results([smoke, other], fn dir ->
        Info.run(dir, [suite: "smoke"], @no_color)
      end)

      output = collect_output()

      assert output =~ "Suite: smoke"
      refute output =~ "Suite: stress"
    end
  end
end
