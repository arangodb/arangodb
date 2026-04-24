defmodule ToastTest.DiagnosticsSummaryTest do
  use ExUnit.Case, async: true

  alias ToastTest.DiagnosticsSummary

  defp suite_result(issues \\ [], opts \\ []) do
    %ToastTest.SuiteResult{
      suite: "smoke",
      started_at: ~U[2026-01-01 00:00:00Z],
      finished_at: ~U[2026-01-01 00:01:00Z],
      times_us: %{async: 0, load: 0, run: 60_000_000},
      issues: issues,
      deployments: Keyword.get(opts, :deployments, %{})
    }
  end

  describe "exit_code/1" do
    test "returns 0 for all passed" do
      assert DiagnosticsSummary.exit_code(%{
               test_failures: 0,
               server_crashed: false,
               infrastructure_failure: false,
               sanitizer_errors: false
             }) == 0
    end

    test "returns 1 for test failures" do
      assert DiagnosticsSummary.exit_code(%{
               test_failures: 3,
               server_crashed: false,
               infrastructure_failure: false,
               sanitizer_errors: false
             }) == 1
    end

    test "returns 2 for sanitizer errors" do
      assert DiagnosticsSummary.exit_code(%{
               test_failures: 0,
               server_crashed: false,
               infrastructure_failure: false,
               sanitizer_errors: true
             }) == 2
    end

    test "returns 3 for infrastructure failure" do
      assert DiagnosticsSummary.exit_code(%{
               test_failures: 0,
               server_crashed: false,
               infrastructure_failure: true,
               sanitizer_errors: false
             }) == 3
    end

    test "returns 4 for server crash" do
      assert DiagnosticsSummary.exit_code(%{
               test_failures: 0,
               server_crashed: true,
               infrastructure_failure: false,
               sanitizer_errors: false
             }) == 4
    end

    test "highest severity wins" do
      assert DiagnosticsSummary.exit_code(%{
               test_failures: 1,
               server_crashed: true,
               infrastructure_failure: true,
               sanitizer_errors: true
             }) == 4

      assert DiagnosticsSummary.exit_code(%{
               test_failures: 0,
               server_crashed: false,
               infrastructure_failure: true,
               sanitizer_errors: true
             }) == 3

      assert DiagnosticsSummary.exit_code(%{
               test_failures: 5,
               server_crashed: false,
               infrastructure_failure: false,
               sanitizer_errors: true
             }) == 2
    end
  end

  describe "has_sanitizer_errors?/1" do
    test "returns false for empty suite list" do
      refute DiagnosticsSummary.has_sanitizer_errors?([])
    end

    test "returns false for suite without suite_result" do
      refute DiagnosticsSummary.has_sanitizer_errors?([%{suite_module: MyApp.Test}])
    end

    test "returns false when no sanitizer issues" do
      result = suite_result([%{type: :test_failure, scope: :suite, confidence: nil, detail: %{}}])

      refute DiagnosticsSummary.has_sanitizer_errors?([%{suite_result: result}])
    end

    test "returns true when sanitizer issues exist" do
      result =
        suite_result([
          %{type: :sanitizer_report, scope: :suite, confidence: nil, detail: %{server: "s1"}}
        ])

      assert DiagnosticsSummary.has_sanitizer_errors?([%{suite_result: result}])
    end

    test "returns true if any suite has sanitizer errors" do
      clean = suite_result()

      dirty =
        suite_result([
          %{type: :sanitizer_report, scope: :suite, confidence: nil, detail: %{server: "s1"}}
        ])

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

    test "no suite_result produces empty fields" do
      suites = [%{suite_module: MyApp.Test}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)

      assert diag.log_files == []
      assert diag.sanitizer_files == []
      assert diag.core_dumps == []
    end

    # --- Core dumps ---

    test "extracts core dump paths from crash issues" do
      result =
        suite_result([
          %{
            type: :crash,
            scope: :suite,
            confidence: nil,
            detail: %{server: "s1", coredump_paths: ["/cores/core.1234"]}
          }
        ])

      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert diag.core_dumps == ["/cores/core.1234"]
    end

    test "extracts multiple core dump paths" do
      result =
        suite_result([
          %{
            type: :crash,
            scope: :suite,
            confidence: nil,
            detail: %{server: "s1", coredump_paths: ["/cores/core.1"]}
          },
          %{
            type: :crash,
            scope: :suite,
            confidence: nil,
            detail: %{server: "s2", coredump_paths: ["/cores/core.2"]}
          }
        ])

      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert Enum.sort(diag.core_dumps) == ["/cores/core.1", "/cores/core.2"]
    end

    test "crash without coredump_path is skipped" do
      result =
        suite_result([
          %{type: :crash, scope: :suite, confidence: nil, detail: %{server: "s1"}}
        ])

      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert diag.core_dumps == []
    end

    # --- Log files ---

    test "extracts log files from deployments" do
      deployments = %{
        "deploy-1" => %{
          servers: %{
            "dbserver-0" => %{log_file: "/tmp/dbserver-0/arangod.log"},
            "coordinator-0" => %{log_file: "/tmp/coordinator-0/arangod.log"}
          }
        }
      }

      result = suite_result([], deployments: deployments)
      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)

      assert Enum.sort(diag.log_files) == [
               "/tmp/coordinator-0/arangod.log",
               "/tmp/dbserver-0/arangod.log"
             ]
    end

    test "handles non-map deployments gracefully" do
      # Guard in extract_log_files/1 falls back to [] for unexpected input.
      result = %ToastTest.SuiteResult{
        suite: "smoke",
        started_at: ~U[2026-01-01 00:00:00Z],
        finished_at: ~U[2026-01-01 00:01:00Z],
        times_us: %{async: 0, load: 0, run: 0},
        issues: [],
        deployments: nil
      }

      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)

      assert diag.log_files == []
    end

    test "skips servers with nil log_file" do
      deployments = %{
        "deploy-1" => %{
          servers: %{
            "dbserver-0" => %{log_file: "/tmp/dbserver-0/arangod.log"},
            "agent-0" => %{log_file: nil}
          }
        }
      }

      result = suite_result([], deployments: deployments)
      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)

      assert diag.log_files == ["/tmp/dbserver-0/arangod.log"]
    end

    # --- Sanitizer files ---

    test "extracts sanitizer file paths from issues" do
      result =
        suite_result([
          %{
            type: :sanitizer_report,
            scope: :suite,
            confidence: nil,
            detail: %{server: "s1", file: "/tmp/s1/alubsan.log.1", report: "ERROR"}
          }
        ])

      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert diag.sanitizer_files == ["/tmp/s1/alubsan.log.1"]
    end

    test "deduplicates sanitizer file paths" do
      result =
        suite_result([
          %{
            type: :sanitizer_report,
            scope: :suite,
            confidence: nil,
            detail: %{server: "s1", file: "/tmp/san.log.1", report: "ERROR 1"}
          },
          %{
            type: :sanitizer_report,
            scope: :suite,
            confidence: nil,
            detail: %{server: "s1", file: "/tmp/san.log.1", report: "ERROR 2"}
          }
        ])

      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert diag.sanitizer_files == ["/tmp/san.log.1"]
    end

    test "sanitizer issue without file field is skipped" do
      result =
        suite_result([
          %{
            type: :sanitizer_report,
            scope: :suite,
            confidence: nil,
            detail: %{server: "s1", report: "ERROR"}
          }
        ])

      suites = [%{suite_module: MyApp.Test, suite_result: result}]
      [diag] = DiagnosticsSummary.build_suite_diagnostics(suites)
      assert diag.sanitizer_files == []
    end
  end
end
