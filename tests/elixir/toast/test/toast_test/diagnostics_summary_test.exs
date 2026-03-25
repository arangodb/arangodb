defmodule ToastTest.DiagnosticsSummaryTest do
  use ExUnit.Case, async: true

  alias ToastTest.DiagnosticsSummary

  describe "has_sanitizer_errors?/1" do
    test "returns false for empty suite list" do
      refute DiagnosticsSummary.has_sanitizer_errors?([])
    end

    test "returns false for suite without suite_result" do
      refute DiagnosticsSummary.has_sanitizer_errors?([%{suite_module: MyApp.Test}])
    end

    test "returns false when no sanitizer issues" do
      suite_result = %ToastTest.SuiteResult{
        suite: "smoke",
        started_at: ~U[2026-01-01 00:00:00Z],
        finished_at: ~U[2026-01-01 00:01:00Z],
        times_us: %{async: 0, load: 0, run: 60_000_000},
        issues: [%{type: :test_failure, scope: :suite, confidence: nil, detail: %{}}]
      }

      refute DiagnosticsSummary.has_sanitizer_errors?([%{suite_result: suite_result}])
    end

    test "returns true when sanitizer issues exist" do
      suite_result = %ToastTest.SuiteResult{
        suite: "smoke",
        started_at: ~U[2026-01-01 00:00:00Z],
        finished_at: ~U[2026-01-01 00:01:00Z],
        times_us: %{async: 0, load: 0, run: 60_000_000},
        issues: [
          %{type: :sanitizer_report, scope: :suite, confidence: nil, detail: %{server: "s1"}}
        ]
      }

      assert DiagnosticsSummary.has_sanitizer_errors?([%{suite_result: suite_result}])
    end

    test "returns true if any suite has sanitizer errors" do
      clean = %ToastTest.SuiteResult{
        suite: "clean",
        started_at: ~U[2026-01-01 00:00:00Z],
        finished_at: ~U[2026-01-01 00:01:00Z],
        times_us: %{async: 0, load: 0, run: 60_000_000},
        issues: []
      }

      dirty = %ToastTest.SuiteResult{
        suite: "dirty",
        started_at: ~U[2026-01-01 00:00:00Z],
        finished_at: ~U[2026-01-01 00:01:00Z],
        times_us: %{async: 0, load: 0, run: 60_000_000},
        issues: [
          %{type: :sanitizer_report, scope: :suite, confidence: nil, detail: %{server: "s1"}}
        ]
      }

      assert DiagnosticsSummary.has_sanitizer_errors?([
               %{suite_result: clean},
               %{suite_result: dirty}
             ])
    end
  end

  describe "build_suite_diagnostics/1" do
    test "returns empty list for empty suites" do
      assert DiagnosticsSummary.build_suite_diagnostics([]) == []
    end

    test "extracts name from suite_module" do
      suites = [%{suite_module: MyApp.Smoke}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)

      assert diag.name == "MyApp.Smoke"
    end

    test "extracts core dump paths from crash issues" do
      suite_result = %ToastTest.SuiteResult{
        suite: "smoke",
        started_at: ~U[2026-01-01 00:00:00Z],
        finished_at: ~U[2026-01-01 00:01:00Z],
        times_us: %{async: 0, load: 0, run: 60_000_000},
        issues: [
          %{
            type: :crash,
            scope: :suite,
            confidence: nil,
            detail: %{
              server: "s1",
              coredump_paths: ["/cores/core.1234"]
            }
          }
        ]
      }

      suites = [%{suite_module: MyApp.Test, suite_result: suite_result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert diag.core_dumps == ["/cores/core.1234"]
    end

    test "extracts multiple core dump paths" do
      suite_result = %ToastTest.SuiteResult{
        suite: "smoke",
        started_at: ~U[2026-01-01 00:00:00Z],
        finished_at: ~U[2026-01-01 00:01:00Z],
        times_us: %{async: 0, load: 0, run: 60_000_000},
        issues: [
          %{
            type: :crash,
            scope: :suite,
            confidence: nil,
            detail: %{
              server: "s1",
              coredump_paths: ["/cores/core.1"]
            }
          },
          %{
            type: :crash,
            scope: :suite,
            confidence: nil,
            detail: %{
              server: "s2",
              coredump_paths: ["/cores/core.2"]
            }
          }
        ]
      }

      suites = [%{suite_module: MyApp.Test, suite_result: suite_result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert Enum.sort(diag.core_dumps) == ["/cores/core.1", "/cores/core.2"]
    end

    test "no suite_result produces empty core_dumps" do
      suites = [%{suite_module: MyApp.Test}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)

      assert diag.core_dumps == []
      assert Map.keys(diag) |> Enum.sort() == [:core_dumps, :name]
    end

    test "crash without coredump_path is skipped" do
      suite_result = %ToastTest.SuiteResult{
        suite: "smoke",
        started_at: ~U[2026-01-01 00:00:00Z],
        finished_at: ~U[2026-01-01 00:01:00Z],
        times_us: %{async: 0, load: 0, run: 60_000_000},
        issues: [
          %{type: :crash, scope: :suite, confidence: nil, detail: %{server: "s1"}}
        ]
      }

      suites = [%{suite_module: MyApp.Test, suite_result: suite_result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert diag.core_dumps == []
    end
  end
end
