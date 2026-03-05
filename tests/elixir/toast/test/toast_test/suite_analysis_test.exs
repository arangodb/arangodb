defmodule ToastTest.SuiteAnalysisTest do
  use ExUnit.Case, async: true

  alias ToastTest.SuiteAnalysis

  @t1 ~U[2026-03-05 10:00:00Z]
  @t2 ~U[2026-03-05 10:00:05Z]
  @t3 ~U[2026-03-05 10:00:10Z]
  @t4 ~U[2026-03-05 10:00:15Z]

  defp crash(timestamp), do: %{timestamp: timestamp}

  defp test_entry(module, name, outcome, started_at) do
    %{module: module, name: name, outcome: outcome, started_at: started_at, finished_at: nil}
  end

  defp wrap_results(tests) do
    grouped =
      tests
      |> Enum.group_by(& &1.module)
      |> Map.new(fn {mod, ts} -> {mod, %{tests: ts}} end)

    %{modules: grouped}
  end

  describe "find_crash_affected_tests/2" do
    test "returns empty list when test_results is nil" do
      crash_matching = %{matched: [%{module: A, test: "t", crash: crash(@t1)}], unmatched: []}
      assert SuiteAnalysis.find_crash_affected_tests(crash_matching, nil) == []
    end

    test "returns empty list when no crashes have timestamps" do
      results = wrap_results([test_entry(A, "t1", :failed, @t1)])
      crash_matching = %{matched: [], unmatched: [crash(nil)]}
      assert SuiteAnalysis.find_crash_affected_tests(crash_matching, results) == []
    end

    test "returns failed tests that started at or after earliest crash" do
      failed_at = test_entry(A, "t1", :failed, @t2)
      failed_exact = test_entry(A, "t2", :failed, @t3)
      results = wrap_results([failed_at, failed_exact])

      crash_matching = %{matched: [], unmatched: [crash(@t2)]}
      affected = SuiteAnalysis.find_crash_affected_tests(crash_matching, results)

      assert length(affected) == 2
      names = Enum.map(affected, & &1.name) |> Enum.sort()
      assert names == ["t1", "t2"]
    end

    test "excludes tests already attributed to a crash via matched entries" do
      attributed = test_entry(A, "t1", :failed, @t2)
      not_attributed = test_entry(A, "t2", :failed, @t3)
      results = wrap_results([attributed, not_attributed])

      crash_matching = %{
        matched: [%{module: A, test: "t1", crash: crash(@t1)}],
        unmatched: []
      }

      affected = SuiteAnalysis.find_crash_affected_tests(crash_matching, results)
      assert [%{name: "t2"}] = affected
    end

    test "excludes passed/skipped tests even if after crash timestamp" do
      passed = test_entry(A, "t1", :passed, @t2)
      skipped = test_entry(A, "t2", :skipped, @t2)
      failed = test_entry(A, "t3", :failed, @t2)
      results = wrap_results([passed, skipped, failed])

      crash_matching = %{matched: [], unmatched: [crash(@t1)]}
      affected = SuiteAnalysis.find_crash_affected_tests(crash_matching, results)

      assert [%{name: "t3"}] = affected
    end

    test "excludes failed tests that started before the crash" do
      before_crash = test_entry(A, "t1", :failed, @t1)
      after_crash = test_entry(A, "t2", :failed, @t3)
      results = wrap_results([before_crash, after_crash])

      crash_matching = %{matched: [], unmatched: [crash(@t2)]}
      affected = SuiteAnalysis.find_crash_affected_tests(crash_matching, results)

      assert [%{name: "t2"}] = affected
    end

    test "handles multiple crashes — uses earliest timestamp" do
      early_fail = test_entry(A, "t1", :failed, @t2)
      late_fail = test_entry(A, "t2", :failed, @t4)
      results = wrap_results([early_fail, late_fail])

      crash_matching = %{
        matched: [%{module: B, test: "other", crash: crash(@t4)}],
        unmatched: [crash(@t2)]
      }

      affected = SuiteAnalysis.find_crash_affected_tests(crash_matching, results)
      names = Enum.map(affected, & &1.name) |> Enum.sort()
      assert names == ["t1", "t2"]
    end

    test "returns empty list when crash_matching has no entries" do
      results = wrap_results([test_entry(A, "t1", :failed, @t1)])
      crash_matching = %{matched: [], unmatched: []}
      assert SuiteAnalysis.find_crash_affected_tests(crash_matching, results) == []
    end
  end
end
