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

  describe "extract_windows/2" do
    test "empty windows list returns []", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] some line"
        ])

      assert Logs.extract_windows([], path) == []
    end

    test "single window extracts matching lines", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] before",
          "2026-01-15T12:00:05Z [1234] INFO [abc13] in window",
          "2026-01-15T12:00:10Z [1234] WARNING [abc14] also in window",
          "2026-01-15T12:00:20Z [1234] INFO [abc15] after"
        ])

      start_dt = dt("2026-01-15T12:00:05Z")
      end_dt = dt("2026-01-15T12:00:10Z")

      [result] = Logs.extract_windows([{start_dt, end_dt}], path)

      assert result =~ "in window"
      assert result =~ "also in window"
      refute result =~ "before"
      refute result =~ "after"
    end

    test "multiple non-overlapping windows each get their own excerpt", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] w1 line1",
          "2026-01-15T12:00:05Z [1234] INFO [abc13] w1 line2",
          "2026-01-15T12:00:15Z [1234] INFO [abc14] gap line",
          "2026-01-15T12:00:20Z [1234] INFO [abc15] w2 line1",
          "2026-01-15T12:00:25Z [1234] INFO [abc16] w2 line2",
          "2026-01-15T12:00:35Z [1234] INFO [abc17] trailing"
        ])

      windows = [
        {dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:00:10Z")},
        {dt("2026-01-15T12:00:20Z"), dt("2026-01-15T12:00:30Z")}
      ]

      [w1, w2] = Logs.extract_windows(windows, path)

      assert w1 =~ "w1 line1"
      assert w1 =~ "w1 line2"
      refute w1 =~ "gap line"
      refute w1 =~ "w2 line"

      assert w2 =~ "w2 line1"
      assert w2 =~ "w2 line2"
      refute w2 =~ "gap line"
      refute w2 =~ "trailing"
    end

    test "line overshooting window 1 appears in window 2", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] w1 line",
          "2026-01-15T12:00:10Z [1234] INFO [abc13] overshoot into w2",
          "2026-01-15T12:00:15Z [1234] INFO [abc14] w2 line"
        ])

      windows = [
        {dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:00:05Z")},
        {dt("2026-01-15T12:00:08Z"), dt("2026-01-15T12:00:20Z")}
      ]

      [w1, w2] = Logs.extract_windows(windows, path)

      assert w1 =~ "w1 line"
      refute w1 =~ "overshoot"

      assert w2 =~ "overshoot into w2"
      assert w2 =~ "w2 line"
    end

    test "non-timestamped continuation lines stay with their window", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] w1 start",
          "  continuation of w1",
          "2026-01-15T12:00:15Z [1234] INFO [abc13] gap",
          "  orphan continuation",
          "2026-01-15T12:00:20Z [1234] INFO [abc14] w2 start",
          "  continuation of w2"
        ])

      windows = [
        {dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:00:05Z")},
        {dt("2026-01-15T12:00:18Z"), dt("2026-01-15T12:00:25Z")}
      ]

      [w1, w2] = Logs.extract_windows(windows, path)

      assert w1 =~ "w1 start"
      assert w1 =~ "continuation of w1"
      refute w1 =~ "orphan"

      assert w2 =~ "w2 start"
      assert w2 =~ "continuation of w2"
      refute w2 =~ "orphan"
    end

    test "file not found returns list of empty strings", _context do
      windows = [
        {dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:00:05Z")},
        {dt("2026-01-15T12:00:10Z"), dt("2026-01-15T12:00:15Z")},
        {dt("2026-01-15T12:00:20Z"), dt("2026-01-15T12:00:25Z")}
      ]

      assert Logs.extract_windows(windows, "/nonexistent/file.log") == ["", "", ""]
    end

    test "empty file returns list of empty strings", %{tmp_dir: dir} do
      path = Path.join(dir, "empty.log")
      File.write!(path, "")

      windows = [
        {dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:00:05Z")},
        {dt("2026-01-15T12:00:10Z"), dt("2026-01-15T12:00:15Z")}
      ]

      assert Logs.extract_windows(windows, path) == ["", ""]
    end

    test "EOF mid-window finalizes current and pads remaining with empty strings", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] w1 line",
          "2026-01-15T12:00:10Z [1234] INFO [abc13] w2 partial"
        ])

      windows = [
        {dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:00:05Z")},
        {dt("2026-01-15T12:00:08Z"), dt("2026-01-15T12:00:15Z")},
        {dt("2026-01-15T12:00:20Z"), dt("2026-01-15T12:00:25Z")}
      ]

      [w1, w2, w3] = Logs.extract_windows(windows, path)

      assert w1 =~ "w1 line"
      assert w2 =~ "w2 partial"
      assert w3 == ""
    end

    test "window with no matching lines returns empty string while others work", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          "2026-01-15T12:00:00Z [1234] INFO [abc12] w1 line",
          "2026-01-15T12:00:05Z [1234] INFO [abc13] w1 line2",
          "2026-01-15T12:00:20Z [1234] INFO [abc14] w3 line"
        ])

      windows = [
        {dt("2026-01-15T12:00:00Z"), dt("2026-01-15T12:00:06Z")},
        {dt("2026-01-15T12:00:10Z"), dt("2026-01-15T12:00:15Z")},
        {dt("2026-01-15T12:00:18Z"), dt("2026-01-15T12:00:25Z")}
      ]

      [w1, w2, w3] = Logs.extract_windows(windows, path)

      assert w1 =~ "w1 line"
      assert w1 =~ "w1 line2"
      assert w2 == ""
      assert w3 =~ "w3 line"
    end
  end
end
