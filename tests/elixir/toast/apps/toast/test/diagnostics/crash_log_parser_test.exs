defmodule Toast.Diagnostics.CrashLogParserTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.CrashLogParser

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

  describe "parse/1" do
    test "parses SIGSEGV crash with backtrace" do
      report = CrashLogParser.parse(@sigsegv_crash_log)

      assert report.signal_number == 11
      assert report.signal_name == "SIGSEGV"
      assert report.crash_header =~ "caught unexpected signal 11"
      assert length(report.backtrace) == 3
      assert Enum.any?(report.backtrace, &(&1 =~ "SomeFunction"))
      assert length(report.fatal_lines) == 2
    end

    test "parses SIGABRT crash" do
      report = CrashLogParser.parse(@sigabrt_crash_log)

      assert report.signal_number == 6
      assert report.signal_name == "SIGABRT"
      assert length(report.backtrace) == 2
    end

    test "returns empty report for clean log" do
      report = CrashLogParser.parse(@clean_log)

      assert report.signal_number == nil
      assert report.signal_name == nil
      assert report.crash_header == nil
      assert report.backtrace == []
      assert report.fatal_lines == []
    end

    test "collects fatal lines from any topic" do
      report = CrashLogParser.parse(@fatal_non_crash)

      assert report.signal_number == nil
      assert report.backtrace == []
      assert length(report.fatal_lines) == 1
      assert hd(report.fatal_lines) =~ "assertion failed"
    end

    test "handles empty string" do
      report = CrashLogParser.parse("")

      assert report.signal_number == nil
      assert report.backtrace == []
      assert report.fatal_lines == []
    end

    test "handles truncated backtrace" do
      log = """
      2024-01-15T10:30:45Z [12345] FATAL [a7902] {crash} caught unexpected signal 11 (SIGSEGV)
      2024-01-15T10:30:45Z [12345] INFO  [308c3] {crash} frame  1 [+0x1234] func1() (+0x10)
      """

      report = CrashLogParser.parse(log)

      assert report.signal_number == 11
      assert length(report.backtrace) == 1
    end
  end

  describe "has_crash?/1" do
    test "returns true for log with crash topic" do
      assert CrashLogParser.has_crash?(@sigsegv_crash_log)
    end

    test "returns false for clean log" do
      refute CrashLogParser.has_crash?(@clean_log)
    end

    test "returns false for empty string" do
      refute CrashLogParser.has_crash?("")
    end

    test "returns false for fatal without crash topic" do
      refute CrashLogParser.has_crash?(@fatal_non_crash)
    end
  end

  describe "format_summary/1" do
    test "formats crash with signal and backtrace" do
      report = CrashLogParser.parse(@sigsegv_crash_log)
      summary = CrashLogParser.format_summary(report)

      assert summary =~ "SIGSEGV"
      assert summary =~ "signal 11"
      assert summary =~ "3"
    end

    test "formats empty report" do
      report = CrashLogParser.parse(@clean_log)
      summary = CrashLogParser.format_summary(report)

      assert summary =~ "No crash"
    end
  end
end
