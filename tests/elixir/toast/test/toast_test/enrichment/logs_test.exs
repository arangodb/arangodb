defmodule ToastTest.Enrichment.LogsTest do
  use ExUnit.Case, async: true

  alias ToastTest.Enrichment.Logs

  @tmp_dir Path.join(System.tmp_dir!(), "toast_logs_test_#{System.unique_integer([:positive])}")

  setup do
    File.mkdir_p!(@tmp_dir)
    on_exit(fn -> File.rm_rf!(@tmp_dir) end)
    {:ok, tmp_dir: @tmp_dir}
  end

  defp write_log(dir, name \\ "arangod.log", lines) do
    path = Path.join(dir, name)
    File.write!(path, Enum.join(lines, "\n") <> "\n")
    path
  end

  defp dt(iso), do: DateTime.from_iso8601(iso) |> elem(1)

  describe "extract_crash_lines/1" do
    test "returns last contiguous block of {crash} lines", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-03-11T15:22:17Z [22788-1] S INFO [abc01] {general} starting up",
          "2026-03-11T15:22:18Z [22788-30] S FATAL [a7902] {crash} caught signal 11",
          "2026-03-11T15:22:18Z [22788-30] S FATAL [a7903] {crash} Hello crash handler",
          "2026-03-11T15:22:19Z [22788-30] S INFO [a7904] {general} shutting down"
        ])

      result = Logs.extract_crash_lines(path)

      assert result =~ "caught signal 11"
      assert result =~ "Hello crash handler"
      refute result =~ "starting up"
      refute result =~ "shutting down"
    end

    test "returns only the last crash block when multiple exist", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-03-11T15:00:00Z [1000-1] S FATAL [a7902] {crash} first crash signal 6",
          "2026-03-11T15:00:01Z [1000-1] S INFO [c962b] {crash} first backtrace frame 1",
          "2026-03-11T15:00:02Z [1000-1] S INFO [abc01] {general} server restarted",
          "2026-03-11T15:00:10Z [2000-1] S INFO [abc02] {general} normal operation",
          "2026-03-11T15:22:18Z [2000-30] S FATAL [a7902] {crash} second crash signal 11",
          "2026-03-11T15:22:18Z [2000-30] S INFO [c962b] {crash} second backtrace frame 1",
          "2026-03-11T15:22:18Z [2000-30] S INFO [308c3] {crash} second backtrace frame 2",
          "2026-03-11T15:22:18Z [2000-30] S FATAL [a7903] {crash} Hello crash handler"
        ])

      result = Logs.extract_crash_lines(path)

      assert result =~ "second crash signal 11"
      assert result =~ "second backtrace frame 1"
      assert result =~ "second backtrace frame 2"
      assert result =~ "Hello crash handler"
      refute result =~ "first crash"
      refute result =~ "first backtrace"
    end

    test "returns empty string when no {crash} lines", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-03-11T15:22:17Z [22788-1] S INFO [abc01] {general} starting up",
          "2026-03-11T15:22:18Z [22788-1] S FATAL [abc02] {startup} something failed"
        ])

      assert Logs.extract_crash_lines(path) == ""
    end

    test "returns empty string for nonexistent file" do
      assert Logs.extract_crash_lines("/nonexistent/file.log") == ""
    end

    test "returns empty string for empty file", %{tmp_dir: dir} do
      path = Path.join(dir, "empty.log")
      File.write!(path, "")
      assert Logs.extract_crash_lines(path) == ""
    end

    test "handles crash block at very end of file (no trailing non-crash lines)", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-03-11T15:22:17Z [22788-1] S INFO [abc01] {general} starting up",
          "2026-03-11T15:22:18Z [22788-30] S FATAL [a7902] {crash} caught signal 11",
          "2026-03-11T15:22:18Z [22788-30] S FATAL [a7903] {crash} Hello crash handler"
        ])

      result = Logs.extract_crash_lines(path)
      assert result =~ "caught signal 11"
      assert result =~ "Hello crash handler"
      refute result =~ "starting up"
    end

    test "handles file with only crash lines", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-03-11T15:22:18Z [22788-30] S FATAL [a7902] {crash} caught signal 11",
          "2026-03-11T15:22:18Z [22788-30] S FATAL [a7903] {crash} Hello crash handler"
        ])

      result = Logs.extract_crash_lines(path)
      assert result =~ "caught signal 11"
      assert result =~ "Hello crash handler"
    end
  end

  describe "extract_window/3" do
    test "returns lines within time window", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] startup message",
          "2026-01-15T12:00:05Z [1234] INFO [abc13] first event",
          "2026-01-15T12:00:10Z [1234] WARNING [abc14] something happened",
          "2026-01-15T12:00:15Z [1234] INFO [abc15] second event",
          "2026-01-15T12:00:20Z [1234] INFO [abc16] later event"
        ])

      result = Logs.extract_window(path, dt("2026-01-15T12:00:05Z"), dt("2026-01-15T12:00:15Z"))

      assert result =~ "first event"
      assert result =~ "something happened"
      assert result =~ "second event"
      refute result =~ "startup message"
      refute result =~ "later event"
    end

    test "returns empty string for nonexistent file" do
      result =
        Logs.extract_window(
          "/nonexistent/file.log",
          dt("2026-01-15T12:00:00Z"),
          dt("2026-01-15T12:01:00Z")
        )

      assert result == ""
    end

    test "returns empty string when no lines match window", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] early",
          "2026-01-15T12:00:01Z [1234] INFO [abc13] still early"
        ])

      result = Logs.extract_window(path, dt("2026-01-15T13:00:00Z"), dt("2026-01-15T14:00:00Z"))
      assert result == ""
    end

    test "handles exact boundary timestamps (inclusive)", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] before",
          "2026-01-15T12:00:05Z [1234] INFO [abc13] at start",
          "2026-01-15T12:00:10Z [1234] INFO [abc14] at end",
          "2026-01-15T12:00:15Z [1234] INFO [abc15] after"
        ])

      result = Logs.extract_window(path, dt("2026-01-15T12:00:05Z"), dt("2026-01-15T12:00:10Z"))

      assert result =~ "at start"
      assert result =~ "at end"
      refute result =~ "before"
      refute result =~ "after"
    end

    test "stops reading after window end for efficiency", %{tmp_dir: dir} do
      # Generate a large log file - lines after the window should be skipped
      early_lines =
        for i <- 1..100 do
          ts =
            "2026-01-15T11:#{String.pad_leading("#{div(i, 60)}", 2, "0")}:#{String.pad_leading("#{rem(i, 60)}", 2, "0")}Z"

          "#{ts} [1234] INFO [abc12] early line #{i}"
        end

      target_lines = [
        "2026-01-15T12:00:00Z [1234] INFO [abc13] target line"
      ]

      late_lines =
        for i <- 1..100 do
          ts =
            "2026-01-15T13:#{String.pad_leading("#{div(i, 60)}", 2, "0")}:#{String.pad_leading("#{rem(i, 60)}", 2, "0")}Z"

          "#{ts} [1234] INFO [abc14] late line #{i}"
        end

      path = write_log(dir, early_lines ++ target_lines ++ late_lines)

      result = Logs.extract_window(path, dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:00:00Z"))
      assert result =~ "target line"
      refute result =~ "late line"
    end

    test "skips lines without parseable timestamps", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] before",
          "this line has no timestamp",
          "2026-01-15T12:00:05Z [1234] INFO [abc13] in window",
          "another non-timestamp line",
          "2026-01-15T12:00:20Z [1234] INFO [abc14] after"
        ])

      result = Logs.extract_window(path, dt("2026-01-15T12:00:03Z"), dt("2026-01-15T12:00:10Z"))
      assert result =~ "in window"
      refute result =~ "before"
      refute result =~ "after"
      # Non-timestamp lines between window boundaries should be included
      # (they belong to the preceding timestamped line conceptually)
    end

    test "handles empty file", %{tmp_dir: dir} do
      path = Path.join(dir, "empty.log")
      File.write!(path, "")

      result = Logs.extract_window(path, dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:01:00Z"))
      assert result == ""
    end
  end
end
