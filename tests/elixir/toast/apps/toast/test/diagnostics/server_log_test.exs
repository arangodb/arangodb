defmodule Toast.Diagnostics.ServerLogTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.ServerLog

  describe "scan/1" do
    test "returns empty report for clean log" do
      log = """
      2024-01-15T10:30:45Z [12345] INFO  [abc12] {general} Server started
      2024-01-15T10:30:46Z [12345] INFO  [abc13] {requests} Processing request
      """

      report = ServerLog.scan(log)
      assert report.assertion_failures == []
      assert report.warnings == []
    end

    test "extracts assertion failure lines" do
      log = """
      2024-01-15T10:30:45Z [12345] INFO  [abc12] {general} Server started
      2024-01-15T10:30:46Z [12345] ERROR [def34] {assertion} Assertion failed: x > 0 in file.cpp:42
      2024-01-15T10:30:47Z [12345] INFO  [abc13] {general} Continuing
      """

      report = ServerLog.scan(log)
      assert length(report.assertion_failures) == 1
      assert hd(report.assertion_failures) =~ "Assertion failed"
    end

    test "extracts non-crash FATAL lines as warnings" do
      log = """
      2024-01-15T10:30:45Z [12345] FATAL [abc12] {general} Out of memory
      2024-01-15T10:30:46Z [12345] FATAL [abc13] {config} Invalid configuration
      """

      report = ServerLog.scan(log)
      assert length(report.warnings) == 2
      assert Enum.any?(report.warnings, &(&1 =~ "Out of memory"))
      assert Enum.any?(report.warnings, &(&1 =~ "Invalid configuration"))
    end

    test "ignores crash-topic FATAL lines" do
      log = """
      2024-01-15T10:30:45Z [12345] FATAL [a7902] {crash} ArangoDB caught unexpected signal 11
      2024-01-15T10:30:46Z [12345] FATAL [abc12] {general} Fatal error outside crash
      """

      report = ServerLog.scan(log)
      # Only the non-crash FATAL line should appear
      assert length(report.warnings) == 1
      assert hd(report.warnings) =~ "Fatal error outside crash"
    end

    test "handles empty string" do
      report = ServerLog.scan("")
      assert report.assertion_failures == []
      assert report.warnings == []
    end

    test "collects multiple assertion failures" do
      log = """
      2024-01-15T10:30:45Z [12345] ERROR [def34] {assertion} First assertion failed
      2024-01-15T10:30:46Z [12345] ERROR [def35] {assertion} Second assertion failed
      """

      report = ServerLog.scan(log)
      assert length(report.assertion_failures) == 2
    end
  end
end
