defmodule Toast.Diagnostics.LogAnalyzerTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.LogAnalyzer

  defp parse_lines(content), do: content |> String.split("\n") |> LogAnalyzer.parse_stream()

  # --- Crash log fixtures (ported from CrashLogParserTest) ---

  @sigsegv_crash_log """
  2024-01-15T10:30:00Z [12345] INFO  [abc12] {general} ArangoDB starting up
  2024-01-15T10:30:01Z [12345] INFO  [def34] {startup} listening on endpoint tcp://0.0.0.0:8529
  2024-01-15T10:30:45Z [12345] FATAL [a7902] {crash} ArangoDB 3.12.0 enterprise, build-id abc123, thread 1 [main] caught unexpected signal 11 (SIGSEGV, sub type SEGV_MAPERR): address 0x0
  2024-01-15T10:30:45Z [12345] FATAL [a7903] {crash} Hello, this is the dedicated crash handler thread
  2024-01-15T10:30:45Z [12345] INFO  [ded81] {crash} physical memory: 16384, rss usage: 1234567
  2024-01-15T10:30:45Z [12345] INFO  [c962b] {crash} Backtrace on thread 1 [main]:
  2024-01-15T10:30:45Z [12345] INFO  [308c3] {crash} frame  1 [+0x1a2b3c] arangodb::SomeFunction(int, char*) (+0x42)
  2024-01-15T10:30:45Z [12345] INFO  [308c3] {crash} frame  2 [+0x4d5e6f] arangodb::OtherFunction() (+0x18)
  2024-01-15T10:30:45Z [12345] INFO  [308c3] {crash} frame  3 [+0x789abc] main (+0x123)
  """

  @sigabrt_crash_log """
  2024-01-15T10:30:45Z [12345] FATAL [a7902] {crash} ArangoDB 3.12.0, thread 2 [worker] caught unexpected signal 6 (SIGABRT): address 0x0
  2024-01-15T10:30:45Z [12345] INFO  [308c3] {crash} frame  1 [+0x1234] __abort (+0x10)
  2024-01-15T10:30:45Z [12345] INFO  [308c3] {crash} frame  2 [+0x5678] arangodb::doSomething() (+0x20)
  """

  @clean_log """
  2024-01-15T10:30:00Z [12345] INFO  [abc12] {general} ArangoDB starting up
  2024-01-15T10:30:01Z [12345] INFO  [def34] {startup} listening on endpoint tcp://0.0.0.0:8529
  2024-01-15T10:30:02Z [12345] INFO  [ghi56] {general} server ready
  """

  @fatal_non_crash """
  2024-01-15T10:30:00Z [12345] INFO  [abc12] {general} ArangoDB starting up
  2024-01-15T10:30:45Z [12345] FATAL [xyz99] {general} assertion failed: x > 0
  """

  # --- Crash parsing tests (ported from CrashLogParserTest) ---

  describe "parse/1 crash analysis" do
    test "parses SIGSEGV crash with backtrace" do
      report = parse_lines(@sigsegv_crash_log)

      assert report.signal_number == 11
      assert report.signal_name == "SIGSEGV"
      assert report.crash_header =~ "caught unexpected signal 11"
      assert length(report.backtrace) == 3
      assert Enum.any?(report.backtrace, &(&1 =~ "SomeFunction"))
      assert report.fatal_lines == []
      assert report.timestamp == ~U[2024-01-15 10:30:45Z]
    end

    test "parses SIGABRT crash" do
      report = parse_lines(@sigabrt_crash_log)

      assert report.signal_number == 6
      assert report.signal_name == "SIGABRT"
      assert length(report.backtrace) == 2
      assert report.timestamp == ~U[2024-01-15 10:30:45Z]
    end

    test "returns empty report for clean log" do
      report = parse_lines(@clean_log)

      assert report.signal_number == nil
      assert report.signal_name == nil
      assert report.crash_header == nil
      assert report.backtrace == []
      assert report.fatal_lines == []
      assert report.timestamp == nil
    end

    test "collects fatal lines from any topic" do
      report = parse_lines(@fatal_non_crash)

      assert report.signal_number == nil
      assert report.backtrace == []
      assert length(report.fatal_lines) == 1
      assert hd(report.fatal_lines) =~ "assertion failed"
    end

    test "handles empty string" do
      report = parse_lines("")

      assert report.signal_number == nil
      assert report.backtrace == []
      assert report.fatal_lines == []
      assert report.timestamp == nil
    end

    test "parses new format with severity abbreviation and pid-tid" do
      log = """
      2026-02-21T17:19:17.461624Z [9185-29] S FATAL [a7902] {crash} ArangoDB 3.12.8-devel enterprise, thread 13 [SchedWorker] caught unexpected signal 11 (SIGSEGV, sub type SEGV_MAPERR): address 0x0
      2026-02-21T17:19:17.461657Z [9185-29] S FATAL [a7903] {crash} Hello, this is the dedicated crash handler thread
      2026-02-21T17:19:17.461700Z [9185-29] S INFO  [308c3] {crash} frame  1 [+0x1a2b3c] arangodb::SomeFunction() (+0x42)
      """

      report = parse_lines(log)

      assert report.signal_number == 11
      assert report.signal_name == "SIGSEGV"
      assert report.crash_header =~ "caught unexpected signal 11"
      assert length(report.backtrace) == 1
      assert report.timestamp == ~U[2026-02-21 17:19:17.461624Z]
    end

    test "collects fatal lines with severity abbreviation format" do
      log = """
      2026-02-21T10:30:00Z [9185-1] S INFO  [abc12] {general} ArangoDB starting up
      2026-02-21T10:30:45Z [9185-1] S FATAL [xyz99] {general} assertion failed: x > 0
      """

      report = parse_lines(log)

      assert report.signal_number == nil
      assert length(report.fatal_lines) == 1
      assert hd(report.fatal_lines) =~ "assertion failed"
    end

    test "handles truncated backtrace" do
      log = """
      2024-01-15T10:30:45Z [12345] FATAL [a7902] {crash} caught unexpected signal 11 (SIGSEGV)
      2024-01-15T10:30:45Z [12345] INFO  [308c3] {crash} frame  1 [+0x1234] func1() (+0x10)
      """

      report = parse_lines(log)

      assert report.signal_number == 11
      assert length(report.backtrace) == 1
      assert report.timestamp == ~U[2024-01-15 10:30:45Z]
    end
  end

  # --- has_crash?/1 tests (ported from CrashLogParserTest) ---

  describe "has_crash?/1" do
    test "returns true for log with crash topic" do
      assert LogAnalyzer.has_crash?(@sigsegv_crash_log)
    end

    test "returns false for clean log" do
      refute LogAnalyzer.has_crash?(@clean_log)
    end

    test "returns false for empty string" do
      refute LogAnalyzer.has_crash?("")
    end

    test "returns false for fatal without crash topic" do
      refute LogAnalyzer.has_crash?(@fatal_non_crash)
    end
  end

  # --- format_summary/1 tests (ported from CrashLogParserTest) ---

  describe "format_summary/1" do
    test "formats crash with signal and backtrace" do
      report = parse_lines(@sigsegv_crash_log)
      summary = LogAnalyzer.format_summary(report)

      assert summary =~ "SIGSEGV"
      assert summary =~ "signal 11"
      assert summary =~ "3"
    end

    test "formats empty report" do
      report = parse_lines(@clean_log)
      summary = LogAnalyzer.format_summary(report)

      assert summary =~ "No crash"
    end
  end

  # --- Server log tests (ported from ServerLogTest) ---

  describe "parse/1 assertion failures" do
    test "returns empty assertions for clean log" do
      report = parse_lines(@clean_log)
      assert report.assertion_failures == []
    end

    test "extracts assertion failure lines" do
      log = """
      2024-01-15T10:30:45Z [12345] INFO  [abc12] {general} Server started
      2024-01-15T10:30:46Z [12345] ERROR [def34] {assertion} Assertion failed: x > 0 in file.cpp:42
      2024-01-15T10:30:47Z [12345] INFO  [abc13] {general} Continuing
      """

      report = parse_lines(log)
      assert length(report.assertion_failures) == 1
      assert hd(report.assertion_failures).message =~ "Assertion failed"
    end

    test "extracts timestamp from assertion failures" do
      log = """
      2024-01-15T10:30:46Z [12345] ERROR [def34] {assertion} Assertion failed: x > 0
      """

      report = parse_lines(log)
      assert [entry] = report.assertion_failures
      assert entry.timestamp == ~U[2024-01-15 10:30:46Z]
      assert entry.message =~ "Assertion failed"
    end

    test "collects multiple assertion failures" do
      log = """
      2024-01-15T10:30:45Z [12345] ERROR [def34] {assertion} First assertion failed
      2024-01-15T10:30:46Z [12345] ERROR [def35] {assertion} Second assertion failed
      """

      report = parse_lines(log)
      assert length(report.assertion_failures) == 2
    end
  end

  describe "parse/1 warnings (broadened capture)" do
    test "returns empty warnings for clean log (all INFO)" do
      report = parse_lines(@clean_log)
      assert report.warnings == []
    end

    test "captures FATAL non-crash lines as warnings" do
      log = """
      2024-01-15T10:30:45Z [12345] FATAL [abc12] {general} Out of memory
      2024-01-15T10:30:46Z [12345] FATAL [abc13] {config} Invalid configuration
      """

      report = parse_lines(log)
      assert length(report.warnings) == 2
      messages = Enum.map(report.warnings, & &1.message)
      assert Enum.any?(messages, &(&1 =~ "Out of memory"))
      assert Enum.any?(messages, &(&1 =~ "Invalid configuration"))
    end

    test "captures ERROR lines as warnings" do
      log = """
      2024-01-15T10:30:45Z [12345] ERROR [abc12] {general} Something bad
      """

      report = parse_lines(log)
      assert length(report.warnings) == 1
      assert hd(report.warnings).message =~ "Something bad"
    end

    test "captures WARNING lines as warnings" do
      log = """
      2024-01-15T10:30:45Z [12345] WARNING [abc12] {general} Disk almost full
      """

      report = parse_lines(log)
      assert length(report.warnings) == 1
      assert hd(report.warnings).message =~ "Disk almost full"
    end

    test "captures DEBUG lines as warnings (non-INFO)" do
      log = """
      2024-01-15T10:30:45Z [12345] DEBUG [abc12] {general} Debug info
      """

      report = parse_lines(log)
      assert length(report.warnings) == 1
      assert hd(report.warnings).message =~ "Debug info"
    end

    test "ignores crash-topic FATAL lines (handled by crash collector)" do
      log = """
      2024-01-15T10:30:45Z [12345] FATAL [a7902] {crash} ArangoDB caught unexpected signal 11
      2024-01-15T10:30:46Z [12345] FATAL [abc12] {general} Fatal error outside crash
      """

      report = parse_lines(log)
      messages = Enum.map(report.warnings, & &1.message)
      assert length(report.warnings) == 1
      assert hd(messages) =~ "Fatal error outside crash"
    end

    test "ignores assertion-topic lines (handled by assertion collector)" do
      log = """
      2024-01-15T10:30:45Z [12345] ERROR [def34] {assertion} Assertion failed
      2024-01-15T10:30:46Z [12345] ERROR [abc12] {general} Regular error
      """

      report = parse_lines(log)
      assert length(report.warnings) == 1
      assert hd(report.warnings).message =~ "Regular error"
      assert length(report.assertion_failures) == 1
    end

    test "extracts timestamp from warnings" do
      log = """
      2024-01-15T10:30:45Z [12345] ERROR [abc12] {general} Something bad
      """

      report = parse_lines(log)
      assert [entry] = report.warnings
      assert entry.timestamp == ~U[2024-01-15 10:30:45Z]
    end

    test "handles empty string" do
      report = parse_lines("")
      assert report.assertion_failures == []
      assert report.warnings == []
    end
  end

  describe "parse/1 uninteresting topic filtering" do
    test "filters out uninteresting topic IDs" do
      log = """
      2024-01-15T10:30:45Z [12345] ERROR [de8f3] {general} Uninteresting error 1
      2024-01-15T10:30:46Z [12345] ERROR [e8b68] {general} Uninteresting error 2
      2024-01-15T10:30:47Z [12345] ERROR [1afb1] {general} Uninteresting error 3
      2024-01-15T10:30:48Z [12345] ERROR [d72fb] {general} Uninteresting error 4
      2024-01-15T10:30:49Z [12345] ERROR [f3108] {general} Uninteresting error 5
      2024-01-15T10:30:50Z [12345] ERROR [abc12] {general} Interesting error
      """

      report = parse_lines(log)
      assert length(report.warnings) == 1
      assert hd(report.warnings).message =~ "Interesting error"
    end

    test "uninteresting topics do not affect assertion or crash collection" do
      log = """
      2024-01-15T10:30:45Z [12345] ERROR [de8f3] {assertion} Assertion with uninteresting topic
      2024-01-15T10:30:46Z [12345] FATAL [de8f3] {crash} caught unexpected signal 11 (SIGSEGV)
      """

      report = parse_lines(log)
      assert length(report.assertion_failures) == 1
      assert report.signal_number == 11
    end
  end

  describe "parse/1 'WARNING about to execute:' filtering" do
    test "filters out 'WARNING about to execute:' lines" do
      log = """
      2024-01-15T10:30:45Z [12345] WARNING [abc12] {general} WARNING about to execute: some command
      2024-01-15T10:30:46Z [12345] ERROR [abc13] {general} Real error
      """

      report = parse_lines(log)
      assert length(report.warnings) == 1
      assert hd(report.warnings).message =~ "Real error"
    end
  end

  describe "parse/1 non-log line filtering" do
    test "ignores lines that don't start with a timestamp" do
      log = """
      Some random output
      2024-01-15T10:30:45Z [12345] ERROR [abc12] {general} Real error
      --- separator ---
      """

      report = parse_lines(log)
      assert length(report.warnings) == 1
      assert hd(report.warnings).message =~ "Real error"
    end
  end

  describe "parse/1 combined single-pass" do
    test "collects crash, assertions, and warnings in one pass" do
      log = """
      2024-01-15T10:30:00Z [12345] INFO  [abc12] {general} ArangoDB starting up
      2024-01-15T10:30:10Z [12345] ERROR [def34] {assertion} Assertion failed: invariant broken
      2024-01-15T10:30:20Z [12345] ERROR [ghi56] {general} Database corruption detected
      2024-01-15T10:30:30Z [12345] FATAL [xyz99] {general} Out of memory
      2024-01-15T10:30:45Z [12345] FATAL [a7902] {crash} caught unexpected signal 11 (SIGSEGV)
      2024-01-15T10:30:45Z [12345] INFO  [308c3] {crash} frame  1 [+0x1234] func() (+0x10)
      """

      report = parse_lines(log)

      # Crash analysis
      assert report.signal_number == 11
      assert report.signal_name == "SIGSEGV"
      assert length(report.backtrace) == 1

      # Assertion failures
      assert length(report.assertion_failures) == 1
      assert hd(report.assertion_failures).message =~ "invariant broken"

      # Warnings (ERROR and FATAL non-crash, non-assertion)
      assert length(report.warnings) == 2
      messages = Enum.map(report.warnings, & &1.message)
      assert Enum.any?(messages, &(&1 =~ "Database corruption"))
      assert Enum.any?(messages, &(&1 =~ "Out of memory"))

      # Fatal lines (FATAL non-crash, bare strings)
      assert length(report.fatal_lines) == 1
      assert hd(report.fatal_lines) =~ "Out of memory"
    end
  end

  describe "finalize/1 removes internal fields" do
    test "uninteresting_topics is not present in finalized report" do
      report = parse_lines("")
      refute Map.has_key?(report, :uninteresting_topics)
    end
  end
end
