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

defmodule ToastTest.Formatting.RunSummaryTest do
  use ExUnit.Case, async: true

  import ExUnit.CaptureIO

  alias ToastTest.SuiteResult
  alias ToastTest.Formatting.RunSummary

  defp strip_ansi(text), do: String.replace(text, ~r/\e\[[0-9;]*m/, "")

  defp make_test(outcome), do: %{name: :test, outcome: outcome, duration_us: 0}

  defp make_module(outcomes) do
    %{
      started_at: DateTime.utc_now(),
      finished_at: DateTime.utc_now(),
      setup_finished_at: nil,
      teardown_started_at: nil,
      tests: Enum.map(outcomes, &make_test/1)
    }
  end

  defp make_suite(modules_map) do
    modules =
      modules_map
      |> Enum.with_index()
      |> Map.new(fn {{_name, outcomes}, idx} ->
        mod = :"Elixir.Mod#{idx}"
        {mod, make_module(outcomes)}
      end)

    %SuiteResult{
      suite: "test_suite",
      started_at: DateTime.utc_now(),
      finished_at: DateTime.utc_now(),
      times_us: %{async: nil, load: nil, run: 0},
      modules: modules
    }
  end

  describe "print/1" do
    test "all passing" do
      suite = make_suite([{"a", [:passed, :passed]}, {"b", [:passed]}])

      output = capture_io(fn -> RunSummary.print([suite], 0) end) |> strip_ansi()

      assert output =~ "Modules:     2 total, 2 passed, 0 failed"
      refute output =~ "mixed"
      refute output =~ "skipped"
      assert output =~ "Test cases:  3 total, 3 passed, 0 failed"
    end

    test "mixed outcomes" do
      suite =
        make_suite([
          {"passing", [:passed, :passed]},
          {"failing", [:passed, :failed]},
          {"skipped", [:skipped, :excluded]}
        ])

      output = capture_io(fn -> RunSummary.print([suite], 0) end) |> strip_ansi()

      assert output =~ "Modules:     3 total, 1 passed, 1 mixed, 0 failed, 1 skipped"
      assert output =~ "Test cases:  6 total, 3 passed, 1 failed, 2 skipped"
    end

    test "multiple suites" do
      suite1 = make_suite([{"a", [:passed, :failed]}])
      suite2 = make_suite([{"b", [:passed]}, {"c", [:skipped]}])

      output = capture_io(fn -> RunSummary.print([suite1, suite2], 0) end) |> strip_ansi()

      assert output =~ "Modules:     3 total, 1 passed, 1 mixed, 0 failed, 1 skipped"
      assert output =~ "Test cases:  4 total, 2 passed, 1 failed, 1 skipped"
    end

    test "empty suite" do
      suite = make_suite([])

      output = capture_io(fn -> RunSummary.print([suite], 0) end) |> strip_ansi()

      assert output =~ "Modules:     0 total, 0 passed, 0 failed"
      assert output =~ "Test cases:  0 total, 0 passed, 0 failed"
    end

    test "no suites" do
      output = capture_io(fn -> RunSummary.print([], 0) end) |> strip_ansi()

      assert output =~ "Modules:     0 total, 0 passed, 0 failed"
      assert output =~ "Test cases:  0 total, 0 passed, 0 failed"
      assert output =~ "Runtime:     0µs"
    end

    test "runtime formatting" do
      suite = make_suite([{"a", [:passed]}])

      output = capture_io(fn -> RunSummary.print([suite], 72_500_000) end) |> strip_ansi()

      assert output =~ "Runtime:     1m12.5s"
    end

    test "module with only failing tests counts as failed" do
      suite = make_suite([{"failing", [:failed, :failed]}])

      output = capture_io(fn -> RunSummary.print([suite], 0) end) |> strip_ansi()

      assert output =~ "Modules:     1 total, 0 passed, 1 failed"
      assert output =~ "Test cases:  2 total, 0 passed, 2 failed"
    end

    test "module with passed and skipped tests (no failures) counts as mixed" do
      suite = make_suite([{"mixed", [:passed, :skipped]}])

      output = capture_io(fn -> RunSummary.print([suite], 0) end) |> strip_ansi()

      assert output =~ "Modules:     1 total, 0 passed, 1 mixed, 0 failed"
      assert output =~ "Test cases:  2 total, 1 passed, 0 failed, 1 skipped"
    end

    test "module with failed and skipped tests (no passes) counts as mixed" do
      suite = make_suite([{"mixed", [:failed, :skipped]}])

      output = capture_io(fn -> RunSummary.print([suite], 0) end) |> strip_ansi()

      assert output =~ "Modules:     1 total, 0 passed, 1 mixed, 0 failed"
      assert output =~ "Test cases:  2 total, 0 passed, 1 failed, 1 skipped"
    end

    test "module with only a single skipped test counts as skipped" do
      suite = make_suite([{"skipped", [:skipped]}])

      output = capture_io(fn -> RunSummary.print([suite], 0) end) |> strip_ansi()

      assert output =~ "Modules:     1 total, 0 passed, 0 failed, 1 skipped"
      assert output =~ "Test cases:  1 total, 0 passed, 0 failed, 1 skipped"
    end

    test "non-pass non-fail outcomes (e.g. :excluded) are classified as skipped" do
      suite = make_suite([{"excluded_only", [:excluded, :excluded]}])

      output = capture_io(fn -> RunSummary.print([suite], 0) end) |> strip_ansi()

      assert output =~ "Modules:     1 total, 0 passed, 0 failed, 1 skipped"
      assert output =~ "Test cases:  2 total, 0 passed, 0 failed, 2 skipped"
    end
  end
end
