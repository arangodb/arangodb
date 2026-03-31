defmodule ToastTest.RunSummaryTest do
  use ExUnit.Case, async: true

  import ExUnit.CaptureIO

  alias ToastTest.{RunSummary, SuiteResult}

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

      output = capture_io(fn -> RunSummary.print([suite]) end) |> strip_ansi()

      assert output =~ "Modules:     2 total, 2 successful, 0 failed, 0 skipped"
      assert output =~ "Test cases:  3 total, 3 successful, 0 failed, 0 skipped"
    end

    test "mixed outcomes" do
      suite =
        make_suite([
          {"passing", [:passed, :passed]},
          {"failing", [:passed, :failed]},
          {"skipped", [:skipped, :excluded]}
        ])

      output = capture_io(fn -> RunSummary.print([suite]) end) |> strip_ansi()

      assert output =~ "Modules:     3 total, 2 successful, 1 failed, 1 skipped"
      assert output =~ "Test cases:  6 total, 4 successful, 1 failed, 2 skipped"
    end

    test "multiple suites" do
      suite1 = make_suite([{"a", [:passed, :failed]}])
      suite2 = make_suite([{"b", [:passed]}, {"c", [:skipped]}])

      output = capture_io(fn -> RunSummary.print([suite1, suite2]) end) |> strip_ansi()

      assert output =~ "Modules:     3 total, 2 successful, 1 failed, 1 skipped"
      assert output =~ "Test cases:  4 total, 3 successful, 1 failed, 1 skipped"
    end

    test "empty suite" do
      suite = make_suite([])

      output = capture_io(fn -> RunSummary.print([suite]) end) |> strip_ansi()

      assert output =~ "Modules:     0 total, 0 successful, 0 failed, 0 skipped"
      assert output =~ "Test cases:  0 total, 0 successful, 0 failed, 0 skipped"
    end

    test "no suites" do
      output = capture_io(fn -> RunSummary.print([]) end) |> strip_ansi()

      assert output =~ "Modules:     0 total, 0 successful, 0 failed, 0 skipped"
      assert output =~ "Test cases:  0 total, 0 successful, 0 failed, 0 skipped"
    end
  end
end
