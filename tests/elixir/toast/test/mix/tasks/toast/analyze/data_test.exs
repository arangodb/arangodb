defmodule Mix.Tasks.Toast.Analyze.DataTest do
  use ExUnit.Case, async: true

  alias Mix.Tasks.Toast.Analyze.Data

  # --- format_server/1 ---

  describe "format_server/1" do
    test "crash issue returns server field" do
      issue = %{type: :crash, detail: %{server: "dbserver1"}}
      assert Data.format_server(issue) == "dbserver1"
    end

    test "sanitizer_report issue returns server field" do
      issue = %{type: :sanitizer_report, detail: %{server: "coordinator1"}}
      assert Data.format_server(issue) == "coordinator1"
    end

    test "timeout issue with single server returns server_id" do
      issue = %{
        type: :timeout,
        detail: %{servers: [%{server_id: "dbserver1"}]}
      }

      assert Data.format_server(issue) == "dbserver1"
    end

    test "timeout issue with multiple servers joins server_ids with comma" do
      issue = %{
        type: :timeout,
        detail: %{
          servers: [
            %{server_id: "coordinator1"},
            %{server_id: "dbserver1"},
            %{server_id: "dbserver2"}
          ]
        }
      }

      assert Data.format_server(issue) == "coordinator1, dbserver1, dbserver2"
    end

    test "timeout issue with empty servers list returns empty string" do
      # The timeout clause matches on `is_list(servers)` regardless of length,
      # so Enum.map_join/3 of [] returns "". The em-dash fallback is not reached.
      issue = %{type: :timeout, detail: %{servers: []}}
      assert Data.format_server(issue) == ""
    end

    test "test_failure issue returns em-dash" do
      issue = %{type: :test_failure, detail: %{test: %{}}}
      assert Data.format_server(issue) == "\u2014"
    end

    test "issue with no recognized structure returns em-dash" do
      assert Data.format_server(%{type: :unknown, detail: %{}}) == "\u2014"
    end
  end

  # --- format_scope/1 ---

  describe "format_scope/1" do
    # The suite scope is the only clause that differs from Issues.format_scope/1:
    # Issues.format_scope(:suite) returns nil, but Data.format_scope/1 maps nil to ":suite".
    test "suite scope returns string :suite fallback" do
      assert Data.format_scope(%{scope: :suite}) == ":suite"
    end

    test "module scope delegates to Issues.format_scope and returns module name" do
      assert Data.format_scope(%{scope: {:module, MyApp.SomeTest}}) == "MyApp.SomeTest"
    end

    test "test scope delegates to Issues.format_scope and returns module > test name" do
      result = Data.format_scope(%{scope: {:test, MyApp.SomeTest, :"test does something"}})
      assert result == "MyApp.SomeTest > \"does something\""
    end

    test "test scope includes file:line when test_location is present" do
      issue = %{
        scope: {:test, MyApp.SomeTest, :"test does something"},
        test_location: "suites/smoke/test_something.exs:42"
      }

      assert Data.format_scope(issue) ==
               "MyApp.SomeTest > \"does something\" (suites/smoke/test_something.exs:42)"
    end
  end

  # --- attach_time_bounds/2 (tested via collect_issues/2) ---
  #
  # attach_time_bounds/2 is private and has 5 clauses. The only way to exercise it
  # is through collect_issues/2, which accepts a list of pre-loaded result maps.

  defp minimal_result(issues, modules \\ %{}) do
    %{
      suite: "my_suite",
      issues: issues,
      modules: modules,
      deployments: %{},
      events: [],
      coredumps: []
    }
  end

  describe "attach_time_bounds via collect_issues/2 — test_failure clause" do
    test "test_failure with matching module and test with DateTime bounds sets time_bounds tuple" do
      started = ~U[2026-01-01 10:00:00Z]
      finished = ~U[2026-01-01 10:00:05Z]

      issue = %{type: :test_failure, scope: {:test, MyMod, :"test something"}}

      modules = %{
        MyMod => %{
          tests: [
            %{name: :"test something", started_at: started, finished_at: finished}
          ]
        }
      }

      [result] = Data.collect_issues([minimal_result([issue], modules)], [])

      expected_start = DateTime.to_unix(started, :microsecond)
      expected_end = DateTime.to_unix(finished, :microsecond)
      assert result.time_bounds == {expected_start, expected_end}
    end

    test "test_failure with no matching module sets time_bounds to nil" do
      issue = %{type: :test_failure, scope: {:test, MyMod, :"test something"}}
      [result] = Data.collect_issues([minimal_result([issue], %{})], [])
      assert result.time_bounds == nil
    end

    test "test_failure with matching module but test not found sets time_bounds to nil" do
      issue = %{type: :test_failure, scope: {:test, MyMod, :"test something"}}

      modules = %{
        MyMod => %{
          tests: [
            %{
              name: :"test other",
              started_at: ~U[2026-01-01 10:00:00Z],
              finished_at: ~U[2026-01-01 10:00:05Z]
            }
          ]
        }
      }

      [result] = Data.collect_issues([minimal_result([issue], modules)], [])
      assert result.time_bounds == nil
    end

    test "test_failure with matching test but non-DateTime timestamps sets time_bounds to nil" do
      issue = %{type: :test_failure, scope: {:test, MyMod, :"test something"}}

      modules = %{
        MyMod => %{tests: [%{name: :"test something", started_at: nil, finished_at: nil}]}
      }

      [result] = Data.collect_issues([minimal_result([issue], modules)], [])
      assert result.time_bounds == nil
    end
  end

  describe "attach_time_bounds via collect_issues/2 — crash clause" do
    test "crash with integer timestamp sets time_bounds to point interval {ts, ts}" do
      ts = 1_700_000_000_000_000
      issue = %{type: :crash, detail: %{crash_info: %{timestamp: ts}, server: "dbserver1"}}
      [result] = Data.collect_issues([minimal_result([issue])], [])
      assert result.time_bounds == {ts, ts}
    end

    test "crash with non-integer timestamp (e.g. DateTime) falls through to nil clause" do
      ts = ~U[2026-01-01 10:00:00Z]
      issue = %{type: :crash, detail: %{crash_info: %{timestamp: ts}, server: "dbserver1"}}
      [result] = Data.collect_issues([minimal_result([issue])], [])
      assert result.time_bounds == nil
    end
  end

  describe "attach_time_bounds via collect_issues/2 — sanitizer_report clause" do
    test "sanitizer_report with integer timestamp sets time_bounds to point interval {ts, ts}" do
      ts = 1_700_000_000_000_000
      issue = %{type: :sanitizer_report, detail: %{timestamp: ts, server: "coordinator1"}}
      [result] = Data.collect_issues([minimal_result([issue])], [])
      assert result.time_bounds == {ts, ts}
    end

    test "sanitizer_report without timestamp falls through to nil clause" do
      issue = %{type: :sanitizer_report, detail: %{server: "coordinator1"}}
      [result] = Data.collect_issues([minimal_result([issue])], [])
      assert result.time_bounds == nil
    end
  end

  describe "attach_time_bounds via collect_issues/2 — timeout clause" do
    test "timeout with integer timestamp sets time_bounds to point interval {ts, ts}" do
      ts = 1_700_000_000_000_000

      issue = %{
        type: :timeout,
        detail: %{timestamp: ts, source: :test_timeout, reason: "timed out", servers: []}
      }

      [result] = Data.collect_issues([minimal_result([issue])], [])
      assert result.time_bounds == {ts, ts}
    end

    test "timeout without timestamp falls through to nil clause" do
      issue = %{
        type: :timeout,
        detail: %{source: :test_timeout, reason: "timed out", servers: []}
      }

      [result] = Data.collect_issues([minimal_result([issue])], [])
      assert result.time_bounds == nil
    end
  end

  describe "attach_time_bounds via collect_issues/2 — catch-all clause" do
    test "unrecognized issue type sets time_bounds to nil" do
      issue = %{type: :test_failure, scope: :suite, detail: %{}}
      [result] = Data.collect_issues([minimal_result([issue])], [])
      assert result.time_bounds == nil
    end
  end

  # --- collect_issues/2 filtering ---

  describe "collect_issues/2 filter_by_type" do
    test "nil type returns all issues" do
      issues = [
        %{type: :crash, detail: %{crash_info: %{timestamp: 1}, server: "s1"}},
        %{type: :test_failure, scope: :suite, detail: %{}}
      ]

      results = Data.collect_issues([minimal_result(issues)], [])
      assert length(results) == 2
    end

    test "type filter keeps only matching issues" do
      issues = [
        %{type: :crash, detail: %{crash_info: %{timestamp: 1}, server: "s1"}},
        %{type: :test_failure, scope: :suite, detail: %{}}
      ]

      results = Data.collect_issues([minimal_result(issues)], type: "crash")
      assert length(results) == 1
      assert hd(results).type == :crash
    end

    test "type filter with no matches returns empty list" do
      issues = [%{type: :test_failure, scope: :suite, detail: %{}}]
      results = Data.collect_issues([minimal_result(issues)], type: "crash")
      assert results == []
    end

    test "unknown type string raises" do
      assert_raise Mix.Error, ~r/Unknown issue type/, fn ->
        Data.collect_issues([minimal_result([])], type: "bogus")
      end
    end
  end

  describe "collect_issues/2 filter_by_suite" do
    test "nil suite returns issues from all results" do
      result_a =
        minimal_result([%{type: :test_failure, scope: :suite, detail: %{}}])
        |> Map.put(:suite, "suite_a")

      result_b =
        minimal_result([%{type: :test_failure, scope: :suite, detail: %{}}])
        |> Map.put(:suite, "suite_b")

      results = Data.collect_issues([result_a, result_b], [])
      assert length(results) == 2
    end

    test "suite filter keeps only issues from matching suite" do
      result_a =
        minimal_result([%{type: :test_failure, scope: :suite, detail: %{}}])
        |> Map.put(:suite, "suite_a")

      result_b =
        minimal_result([%{type: :test_failure, scope: :suite, detail: %{}}])
        |> Map.put(:suite, "suite_b")

      results = Data.collect_issues([result_a, result_b], suite: "suite_a")
      assert length(results) == 1
      assert hd(results).suite == "suite_a"
    end
  end
end
