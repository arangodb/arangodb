defmodule Toast.Diagnostics.CrashMatcherTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.CrashMatcher
  alias Toast.Deployment.ServerInstance

  import Toast.DiagnosticsTestHelpers, only: [at: 1, make_test: 0, make_test: 1, make_test_results: 1]

  defp make_crash_report(opts \\ []) do
    %{
      signal_number: Keyword.get(opts, :signal_number, 11),
      signal_name: Keyword.get(opts, :signal_name, "SIGSEGV"),
      crash_header: Keyword.get(opts, :crash_header, "caught unexpected signal 11 (SIGSEGV)"),
      backtrace: Keyword.get(opts, :backtrace, ["frame 0 at 0xdead"]),
      fatal_lines: Keyword.get(opts, :fatal_lines, []),
      crash_output: Keyword.get(opts, :crash_output, ["caught unexpected signal 11 (SIGSEGV)", "frame 0 at 0xdead"]),
      timestamp: Keyword.get(opts, :timestamp, at(5))
    }
  end

  defp make_diagnostics(opts \\ []) do
    %{
      sanitizer_errors: [],
      server_log: %{assertion_failures: [], warnings: []},
      crash_report: Keyword.get(opts, :crash_report, make_crash_report()),
      server_error: Keyword.get(opts, :server_error, {:server_crashed, %{exit_status: 139, signal: 11, timestamp: at(5)}}),
      server: Keyword.get(opts, :server, %ServerInstance{id: "toast-1", role: :single, log_file: "/tmp/toast/server/log"})
    }
  end

  defp no_crash_report do
    %{
      signal_number: nil,
      signal_name: nil,
      crash_header: nil,
      backtrace: [],
      fatal_lines: [],
      crash_output: [],
      timestamp: nil
    }
  end

  describe "match/3" do
    test "returns empty result for nil diagnostics" do
      assert %{matched: [], unmatched: []} == CrashMatcher.match(nil, make_test_results([]))
    end

    test "returns empty result for nil test_results" do
      assert %{matched: [], unmatched: []} == CrashMatcher.match(make_diagnostics(), nil)
    end

    test "returns empty result when no crash" do
      diag = make_diagnostics(crash_report: no_crash_report(), server_error: nil)
      result = CrashMatcher.match(diag, make_test_results([make_test()]))
      assert result == %{matched: [], unmatched: []}
    end

    test "returns crash as unmatched when crash has no timestamp" do
      crash = make_crash_report(timestamp: nil)
      diag = make_diagnostics(crash_report: crash)
      result = CrashMatcher.match(diag, make_test_results([make_test()]))
      assert result.matched == []
      assert [%{server_id: "toast-1", timestamp: nil}] = result.unmatched
    end

    test "matches crash to test with high confidence when within window" do
      crash = make_crash_report(timestamp: at(5))
      diag = make_diagnostics(crash_report: crash)
      test = make_test(started_at: at(0), finished_at: at(10))

      result = CrashMatcher.match(diag, make_test_results([test]))

      assert [%{confidence: :high, module: SmokeTest.VersionTest, test: "test server version"}] = result.matched
      assert result.unmatched == []
    end

    test "matches crash with low confidence when within tolerance after test end" do
      crash = make_crash_report(timestamp: at(13))
      diag = make_diagnostics(crash_report: crash)
      test = make_test(started_at: at(0), finished_at: at(10))

      result = CrashMatcher.match(diag, make_test_results([test]))

      assert [%{confidence: :low}] = result.matched
      assert result.unmatched == []
    end

    test "returns crash as unmatched when no test correlates" do
      crash = make_crash_report(timestamp: at(30))
      diag = make_diagnostics(crash_report: crash)
      test = make_test(started_at: at(0), finished_at: at(10))

      result = CrashMatcher.match(diag, make_test_results([test]))

      assert result.matched == []
      assert [%{server_id: "toast-1"}] = result.unmatched
    end

    test "prefers high confidence over low confidence match" do
      crash = make_crash_report(timestamp: at(15))
      diag = make_diagnostics(crash_report: crash)
      test1 = make_test(name: "test one", started_at: at(0), finished_at: at(10))
      test2 = make_test(name: "test two", started_at: at(12), finished_at: at(20))

      result = CrashMatcher.match(diag, make_test_results([test1, test2]))

      assert [%{confidence: :high, test: "test two"}] = result.matched
    end

    test "handles tests with missing timestamps" do
      crash = make_crash_report(timestamp: at(5))
      diag = make_diagnostics(crash_report: crash)
      test = make_test(started_at: nil, finished_at: nil)

      result = CrashMatcher.match(diag, make_test_results([test]))

      assert result.matched == []
      assert length(result.unmatched) == 1
    end

    test "respects custom tolerance option" do
      crash = make_crash_report(timestamp: at(12))
      diag = make_diagnostics(crash_report: crash)
      test = make_test(started_at: at(0), finished_at: at(10))

      result_low = CrashMatcher.match(diag, make_test_results([test]), tolerance_seconds: 3)
      assert [%{confidence: :low}] = result_low.matched

      result_none = CrashMatcher.match(diag, make_test_results([test]), tolerance_seconds: 1)
      assert result_none.matched == []
      assert length(result_none.unmatched) == 1
    end

    test "handles cluster diagnostics structure" do
      crash = make_crash_report(timestamp: at(5))

      cluster_diag = %{
        "agent-1" => %{
          sanitizer_errors: [],
          server_log: nil,
          crash_report: no_crash_report(),
          server_error: nil,
          server: %ServerInstance{id: "agent-1", role: :agent}
        },
        "dbserver-1" => %{
          sanitizer_errors: [],
          server_log: nil,
          crash_report: crash,
          server_error: {:server_crashed, %{exit_status: 139, signal: 11, timestamp: at(5)}},
          server: %ServerInstance{id: "dbserver-1", role: :dbserver, log_file: "/tmp/toast/dbserver/log"}
        }
      }

      test = make_test(started_at: at(0), finished_at: at(10))
      result = CrashMatcher.match(cluster_diag, make_test_results([test]))

      assert [%{confidence: :high, crash: %{server_id: "dbserver-1"}}] = result.matched
    end

    test "preserves crash info fields in match entry" do
      crash = make_crash_report(
        signal_name: "SIGABRT",
        signal_number: 6,
        crash_header: "caught unexpected signal 6 (SIGABRT)",
        backtrace: ["frame 0 at 0xbeef", "frame 1 at 0xcafe"],
        fatal_lines: ["fatal error occurred"],
        crash_output: ["caught unexpected signal 6 (SIGABRT)", "frame 0 at 0xbeef"]
      )

      diag = make_diagnostics(
        crash_report: crash,
        server: %ServerInstance{id: "s1", role: :single, log_file: "/tmp/s1.log"}
      )

      test = make_test(started_at: at(0), finished_at: at(10))
      result = CrashMatcher.match(diag, make_test_results([test]))

      assert [entry] = result.matched
      assert entry.crash.server_id == "s1"
      assert entry.crash.signal_name == "SIGABRT"
      assert entry.crash.signal_number == 6
      assert entry.crash.crash_header == "caught unexpected signal 6 (SIGABRT)"
      assert entry.crash.backtrace == ["frame 0 at 0xbeef", "frame 1 at 0xcafe"]
      assert entry.crash.fatal_lines == ["fatal error occurred"]
      assert entry.crash.crash_output == ["caught unexpected signal 6 (SIGABRT)", "frame 0 at 0xbeef"]
      assert entry.crash.log_file == "/tmp/s1.log"
    end
  end
end
