defmodule Toast.Diagnostics.SanitizerMatcherTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.SanitizerMatcher
  alias Toast.Deployment.ServerInstance

  @base_time ~U[2024-06-15 10:00:00Z]

  defp at(seconds), do: DateTime.add(@base_time, seconds, :second)

  defp make_error(opts \\ []) do
    %{
      content: Keyword.get(opts, :content, "==12345==ERROR: AddressSanitizer: heap-buffer-overflow\n"),
      file_path: Keyword.get(opts, :file_path, "/tmp/toast/server/alubsan.log.arangod.12345"),
      timestamp: Keyword.get(opts, :timestamp, at(5)),
      sanitizer_type: Keyword.get(opts, :sanitizer_type, :alubsan),
      server_id: Keyword.get(opts, :server_id, "toast-1")
    }
  end

  defp make_test(opts \\ []) do
    %{
      module: Keyword.get(opts, :module, SmokeTest.VersionTest),
      name: Keyword.get(opts, :name, "test server version"),
      outcome: :passed,
      duration_us: 1_000_000,
      failure: nil,
      started_at: Keyword.get(opts, :started_at, at(0)),
      finished_at: Keyword.get(opts, :finished_at, at(10)),
      tags: %{file: "test/version_test.exs", line: 5}
    }
  end

  defp make_diagnostics(errors) do
    %{
      sanitizer_errors: errors,
      server_log: %{assertion_failures: [], warnings: []},
      crash_report: %{signal_number: nil, signal_name: nil, crash_header: nil, backtrace: [], fatal_lines: [], crash_output: []},
      server_error: nil,
      server: %ServerInstance{id: "toast-1", role: :single}
    }
  end

  defp make_test_results(tests) do
    %{
      suite_started_at: @base_time,
      suite_finished_at: at(60),
      times_us: %{run: 60_000_000, async: nil, load: 100_000},
      tests: tests
    }
  end

  describe "calculate_confidence/4" do
    test "returns :high when timestamp is within test window" do
      assert :high == SanitizerMatcher.calculate_confidence(at(5), at(0), at(10))
    end

    test "returns :high when timestamp equals test start" do
      assert :high == SanitizerMatcher.calculate_confidence(at(0), at(0), at(10))
    end

    test "returns :high when timestamp equals test end" do
      assert :high == SanitizerMatcher.calculate_confidence(at(10), at(0), at(10))
    end

    test "returns :low when timestamp is within tolerance after test end" do
      assert :low == SanitizerMatcher.calculate_confidence(at(13), at(0), at(10))
    end

    test "returns :low at exactly tolerance boundary" do
      assert :low == SanitizerMatcher.calculate_confidence(at(15), at(0), at(10))
    end

    test "returns :none when timestamp is beyond tolerance" do
      assert :none == SanitizerMatcher.calculate_confidence(at(16), at(0), at(10))
    end

    test "returns :none when timestamp is before test start" do
      assert :none == SanitizerMatcher.calculate_confidence(at(-1), at(0), at(10))
    end

    test "respects custom tolerance" do
      assert :low == SanitizerMatcher.calculate_confidence(at(12), at(0), at(10), 3)
      assert :none == SanitizerMatcher.calculate_confidence(at(14), at(0), at(10), 3)
    end
  end

  describe "match/3" do
    test "returns empty result for nil diagnostics" do
      assert %{matched: [], unmatched: []} == SanitizerMatcher.match(nil, make_test_results([]))
    end

    test "returns empty result for nil test_results" do
      assert %{matched: [], unmatched: []} == SanitizerMatcher.match(make_diagnostics([]), nil)
    end

    test "returns empty result when no sanitizer errors" do
      result = SanitizerMatcher.match(make_diagnostics([]), make_test_results([make_test()]))
      assert result == %{matched: [], unmatched: []}
    end

    test "matches error to test with high confidence when within window" do
      error = make_error(timestamp: at(5))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = SanitizerMatcher.match(make_diagnostics([error]), make_test_results([test]))

      assert [%{confidence: :high, module: SmokeTest.VersionTest, test: "test server version"}] = result.matched
      assert result.unmatched == []
    end

    test "matches error with low confidence when within tolerance after test end" do
      error = make_error(timestamp: at(13))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = SanitizerMatcher.match(make_diagnostics([error]), make_test_results([test]))

      assert [%{confidence: :low}] = result.matched
      assert result.unmatched == []
    end

    test "returns error as unmatched when no test correlates" do
      error = make_error(timestamp: at(30))
      test = make_test(started_at: at(0), finished_at: at(10))

      result = SanitizerMatcher.match(make_diagnostics([error]), make_test_results([test]))

      assert result.matched == []
      assert [%{server_id: "toast-1"}] = result.unmatched
    end

    test "prefers high confidence over low confidence" do
      error = make_error(timestamp: at(15))
      test1 = make_test(name: "test one", started_at: at(0), finished_at: at(10))
      test2 = make_test(name: "test two", started_at: at(12), finished_at: at(20))

      result = SanitizerMatcher.match(make_diagnostics([error]), make_test_results([test1, test2]))

      assert [%{confidence: :high, test: "test two"}] = result.matched
    end

    test "matches multiple errors to different tests" do
      error1 = make_error(timestamp: at(5), file_path: "/tmp/san1")
      error2 = make_error(timestamp: at(25), file_path: "/tmp/san2")

      test1 = make_test(name: "test one", started_at: at(0), finished_at: at(10))
      test2 = make_test(name: "test two", started_at: at(20), finished_at: at(30))

      result = SanitizerMatcher.match(make_diagnostics([error1, error2]), make_test_results([test1, test2]))

      assert length(result.matched) == 2
      tests = Enum.map(result.matched, & &1.test)
      assert "test one" in tests
      assert "test two" in tests
    end

    test "handles tests with missing timestamps" do
      error = make_error(timestamp: at(5))
      test = make_test(started_at: nil, finished_at: nil)

      result = SanitizerMatcher.match(make_diagnostics([error]), make_test_results([test]))

      assert result.matched == []
      assert length(result.unmatched) == 1
    end

    test "respects custom tolerance option" do
      error = make_error(timestamp: at(12))
      test = make_test(started_at: at(0), finished_at: at(10))

      result_low = SanitizerMatcher.match(make_diagnostics([error]), make_test_results([test]), tolerance_seconds: 3)
      assert [%{confidence: :low}] = result_low.matched

      result_none = SanitizerMatcher.match(make_diagnostics([error]), make_test_results([test]), tolerance_seconds: 1)
      assert result_none.matched == []
      assert length(result_none.unmatched) == 1
    end

    test "handles cluster diagnostics structure" do
      error = make_error(server_id: "dbserver-1", timestamp: at(5))

      cluster_diag = %{
        "agent-1" => %{
          sanitizer_errors: [],
          server_log: nil,
          crash_report: nil,
          server_error: nil,
          server: %ServerInstance{id: "agent-1", role: :agent}
        },
        "dbserver-1" => %{
          sanitizer_errors: [error],
          server_log: nil,
          crash_report: nil,
          server_error: nil,
          server: %ServerInstance{id: "dbserver-1", role: :dbserver}
        }
      }

      test = make_test(started_at: at(0), finished_at: at(10))
      result = SanitizerMatcher.match(cluster_diag, make_test_results([test]))

      assert [%{confidence: :high, error: %{server_id: "dbserver-1"}}] = result.matched
    end

    test "preserves original error in match entry" do
      error = make_error(content: "ASAN error details", sanitizer_type: :alubsan, server_id: "s1")
      test = make_test(started_at: at(0), finished_at: at(10))

      result = SanitizerMatcher.match(make_diagnostics([error]), make_test_results([test]))

      assert [entry] = result.matched
      assert entry.error.content == "ASAN error details"
      assert entry.error.sanitizer_type == :alubsan
      assert entry.error.server_id == "s1"
    end
  end
end
