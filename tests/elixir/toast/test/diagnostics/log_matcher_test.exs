defmodule Toast.Diagnostics.LogMatcherTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.LogMatcher
  alias Toast.Deployment.ServerInstance

  import Toast.DiagnosticsTestHelpers,
    only: [at: 1, make_test: 0, make_test: 1]

  defp make_log_report(opts \\ []) do
    %{
      signal_number: nil,
      signal_name: nil,
      crash_header: nil,
      backtrace: [],
      fatal_lines: [],
      crash_output: [],
      timestamp: nil,
      assertion_failures: Keyword.get(opts, :assertion_failures, []),
      warnings: Keyword.get(opts, :warnings, [])
    }
  end

  defp make_log_entry(opts \\ []) do
    %{
      timestamp: Keyword.get(opts, :timestamp, at(5)),
      message: Keyword.get(opts, :message, "something happened")
    }
  end

  defp make_diagnostics(server_id \\ "toast-1", opts) do
    server =
      Keyword.get(opts, :server, %ServerInstance{
        id: server_id,
        role: :single,
        log_file: "/tmp/toast/server/log"
      })

    entry = %{
      sanitizer_errors: [],
      log_report: Keyword.get(opts, :log_report, make_log_report()),
      server_error: nil,
      server: server
    }

    %{server.id => entry}
  end

  describe "match/3" do
    test "returns empty result for nil diagnostics" do
      assert %{matched: [], unmatched: []} == LogMatcher.match(nil, [])
    end

    test "returns empty result for nil test_results" do
      diag = make_diagnostics(log_report: make_log_report())
      assert %{matched: [], unmatched: []} == LogMatcher.match(diag, nil)
    end

    test "returns empty result when no log entries" do
      diag = make_diagnostics(log_report: make_log_report())
      result = LogMatcher.match(diag, [make_test()])
      assert result == %{matched: [], unmatched: []}
    end

    test "matches warning to test with high confidence when within window" do
      warning = make_log_entry(timestamp: at(5), message: "disk almost full")
      diag = make_diagnostics(log_report: make_log_report(warnings: [warning]))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = LogMatcher.match(diag, [test])

      assert [%{confidence: :high, log: %{kind: :warning, message: "disk almost full"}}] =
               result.matched

      assert result.unmatched == []
    end

    test "matches assertion failure to test with high confidence" do
      assertion = make_log_entry(timestamp: at(5), message: "invariant broken")
      diag = make_diagnostics(log_report: make_log_report(assertion_failures: [assertion]))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = LogMatcher.match(diag, [test])

      assert [%{confidence: :high, log: %{kind: :assertion, message: "invariant broken"}}] =
               result.matched
    end

    test "matches with low confidence when within tolerance after test end" do
      warning = make_log_entry(timestamp: at(13))
      diag = make_diagnostics(log_report: make_log_report(warnings: [warning]))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = LogMatcher.match(diag, [test])

      assert [%{confidence: :low}] = result.matched
      assert result.unmatched == []
    end

    test "returns entry as unmatched when no test correlates" do
      warning = make_log_entry(timestamp: at(30))
      diag = make_diagnostics(log_report: make_log_report(warnings: [warning]))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = LogMatcher.match(diag, [test])

      assert result.matched == []
      assert [%{server_id: "toast-1", kind: :warning}] = result.unmatched
    end

    test "entries without timestamps go to unmatched" do
      warning = make_log_entry(timestamp: nil, message: "no ts")
      diag = make_diagnostics(log_report: make_log_report(warnings: [warning]))
      test = make_test()

      result = LogMatcher.match(diag, [test])

      assert result.matched == []
      assert [%{timestamp: nil, message: "no ts"}] = result.unmatched
    end

    test "matches both warnings and assertions from the same server" do
      warning = make_log_entry(timestamp: at(3), message: "warn")
      assertion = make_log_entry(timestamp: at(7), message: "assert")

      diag =
        make_diagnostics(
          log_report: make_log_report(warnings: [warning], assertion_failures: [assertion])
        )

      test = make_test(started_at: at(0), finished_at: at(10))

      result = LogMatcher.match(diag, [test])

      assert length(result.matched) == 2
      kinds = Enum.map(result.matched, & &1.log.kind) |> Enum.sort()
      assert kinds == [:assertion, :warning]
    end

    test "handles cluster diagnostics with multiple servers" do
      warning1 = make_log_entry(timestamp: at(5), message: "agent warn")
      warning2 = make_log_entry(timestamp: at(5), message: "dbserver warn")

      diag = %{
        "agent-1" => %{
          sanitizer_errors: [],
          log_report: make_log_report(warnings: [warning1]),
          server_error: nil,
          server: %ServerInstance{id: "agent-1", role: :agent}
        },
        "dbserver-1" => %{
          sanitizer_errors: [],
          log_report: make_log_report(warnings: [warning2]),
          server_error: nil,
          server: %ServerInstance{id: "dbserver-1", role: :dbserver}
        }
      }

      test = make_test(started_at: at(0), finished_at: at(10))
      result = LogMatcher.match(diag, [test])

      assert length(result.matched) == 2

      server_ids =
        result.matched
        |> Enum.map(& &1.log.server_id)
        |> Enum.sort()

      assert server_ids == ["agent-1", "dbserver-1"]
    end

    test "handles nil log_report in diagnostics" do
      diag = %{
        "toast-1" => %{
          sanitizer_errors: [],
          log_report: nil,
          server_error: nil,
          server: %ServerInstance{id: "toast-1", role: :single}
        }
      }

      result = LogMatcher.match(diag, [make_test()])
      assert result == %{matched: [], unmatched: []}
    end

    test "respects custom tolerance option" do
      warning = make_log_entry(timestamp: at(12))
      diag = make_diagnostics(log_report: make_log_report(warnings: [warning]))
      test = make_test(started_at: at(0), finished_at: at(10))

      result_low = LogMatcher.match(diag, [test], tolerance_seconds: 3)
      assert [%{confidence: :low}] = result_low.matched

      result_none = LogMatcher.match(diag, [test], tolerance_seconds: 1)
      assert result_none.matched == []
      assert length(result_none.unmatched) == 1
    end

    test "preserves server_id and message in match entries" do
      warning = make_log_entry(timestamp: at(5), message: "disk issue")

      diag =
        make_diagnostics("srv-42",
          log_report: make_log_report(warnings: [warning]),
          server: %ServerInstance{id: "srv-42", role: :single, log_file: "/tmp/srv-42.log"}
        )

      test = make_test(started_at: at(0), finished_at: at(10))
      result = LogMatcher.match(diag, [test])

      assert [entry] = result.matched
      assert entry.log.server_id == "srv-42"
      assert entry.log.message == "disk issue"
      assert entry.log.kind == :warning
    end
  end
end
