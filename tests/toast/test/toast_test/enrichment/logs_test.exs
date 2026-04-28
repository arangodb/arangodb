################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

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
  defp ts_us(iso), do: dt(iso) |> DateTime.to_unix(:microsecond)

  defp log_json(time, opts) do
    base = %{"time" => time, "message" => opts[:message] || ""}

    base
    |> maybe_add("level", opts[:level])
    |> maybe_add("pid", opts[:pid])
    |> maybe_add("id", opts[:id])
    |> maybe_add("topic", opts[:topic])
    |> maybe_add("role", opts[:role])
    |> :json.encode()
    |> IO.iodata_to_binary()
  end

  defp maybe_add(map, _key, nil), do: map
  defp maybe_add(map, key, val), do: Map.put(map, key, val)

  describe "parse_line/1" do
    test "parses a complete JSON log line" do
      line =
        log_json("2026-03-09T10:00:00.000Z",
          level: "INFO",
          pid: "12345",
          id: "abc01",
          topic: "general",
          role: "S",
          message: "Server started"
        )

      assert {:ok, entry} = Logs.parse_line(line)
      assert entry.time == ts_us("2026-03-09T10:00:00.000Z")
      assert entry.message == "Server started"
      assert entry.level == :info
      assert entry.pid == "12345"
      assert entry.id == "abc01"
      assert entry.topic == :general
      assert entry.role == :single
    end

    test "parses minimal line with only time and message" do
      line = ~s|{"time":"2026-03-09T10:00:00Z","message":"hello"}|

      assert {:ok, entry} = Logs.parse_line(line)
      assert entry.time == ts_us("2026-03-09T10:00:00Z")
      assert entry.message == "hello"
      refute Map.has_key?(entry, :level)
      refute Map.has_key?(entry, :topic)
    end

    test "maps all level strings" do
      for {str, atom} <- [
            {"FATAL", :fatal},
            {"ERR", :error},
            {"WARN", :warning},
            {"INFO", :info},
            {"DEBUG", :debug},
            {"TRACE", :trace}
          ] do
        line = log_json("2026-03-09T10:00:00Z", level: str, message: "x")
        assert {:ok, entry} = Logs.parse_line(line)
        assert entry.level == atom
      end
    end

    test "maps all role strings" do
      for {str, atom} <- [
            {"C", :coordinator},
            {"P", :dbserver},
            {"A", :agent},
            {"S", :single}
          ] do
        line = log_json("2026-03-09T10:00:00Z", role: str, message: "x")
        assert {:ok, entry} = Logs.parse_line(line)
        assert entry.role == atom
      end
    end

    test "returns :error for non-JSON" do
      assert :error = Logs.parse_line("not json at all")
    end

    test "returns :error for JSON missing time field" do
      assert :error = Logs.parse_line(~s|{"message":"no time"}|)
    end

    test "returns :error for invalid time format" do
      assert :error = Logs.parse_line(~s|{"time":"not-a-date","message":"bad"}|)
    end

    test "strips trailing newline before parsing" do
      line = ~s|{"time":"2026-03-09T10:00:00Z","message":"ok"}\n|
      assert {:ok, entry} = Logs.parse_line(line)
      assert entry.message == "ok"
    end

    test "defaults message to empty string when absent" do
      line = ~s|{"time":"2026-03-09T10:00:00Z"}|
      assert {:ok, entry} = Logs.parse_line(line)
      assert entry.message == ""
    end
  end

  describe "extract_crash_lines/1" do
    test "returns last contiguous block of crash-topic entries", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-03-11T15:22:17Z",
            level: "INFO",
            id: "abc01",
            topic: "general",
            message: "starting up"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            id: "a7902",
            topic: "crash",
            message: "caught signal 11"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            id: "a7903",
            topic: "crash",
            message: "Hello crash handler"
          ),
          log_json("2026-03-11T15:22:19Z",
            level: "INFO",
            id: "a7904",
            topic: "general",
            message: "shutting down"
          )
        ])

      result = Logs.extract_crash_lines(path)

      assert length(result) == 2
      assert Enum.any?(result, &(&1.message == "caught signal 11"))
      assert Enum.any?(result, &(&1.message == "Hello crash handler"))
      refute Enum.any?(result, &(&1.message =~ "starting up"))
      refute Enum.any?(result, &(&1.message =~ "shutting down"))
    end

    test "returns only the last crash block when multiple exist", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-03-11T15:00:00Z",
            level: "FATAL",
            topic: "crash",
            message: "first crash signal 6"
          ),
          log_json("2026-03-11T15:00:01Z",
            level: "INFO",
            topic: "crash",
            message: "first backtrace frame 1"
          ),
          log_json("2026-03-11T15:00:02Z",
            level: "INFO",
            topic: "general",
            message: "server restarted"
          ),
          log_json("2026-03-11T15:00:10Z",
            level: "INFO",
            topic: "general",
            message: "normal operation"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            topic: "crash",
            message: "second crash signal 11"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "INFO",
            topic: "crash",
            message: "second backtrace frame 1"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "INFO",
            topic: "crash",
            message: "second backtrace frame 2"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            topic: "crash",
            message: "Hello crash handler"
          )
        ])

      result = Logs.extract_crash_lines(path)

      messages = Enum.map(result, & &1.message)
      assert "second crash signal 11" in messages
      assert "second backtrace frame 1" in messages
      assert "second backtrace frame 2" in messages
      assert "Hello crash handler" in messages
      refute Enum.any?(messages, &String.contains?(&1, "first"))
    end

    test "returns empty list when no crash entries", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-03-11T15:22:17Z",
            level: "INFO",
            topic: "general",
            message: "starting up"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            topic: "startup",
            message: "something failed"
          )
        ])

      assert Logs.extract_crash_lines(path) == []
    end

    test "returns empty list for nonexistent file" do
      assert Logs.extract_crash_lines("/nonexistent/file.log") == []
    end

    test "returns empty list for empty file", %{tmp_dir: dir} do
      path = Path.join(dir, "empty.log")
      File.write!(path, "")
      assert Logs.extract_crash_lines(path) == []
    end

    test "handles crash block at very end of file", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-03-11T15:22:17Z",
            level: "INFO",
            topic: "general",
            message: "starting up"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            topic: "crash",
            message: "caught signal 11"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            topic: "crash",
            message: "Hello crash handler"
          )
        ])

      result = Logs.extract_crash_lines(path)
      messages = Enum.map(result, & &1.message)
      assert "caught signal 11" in messages
      assert "Hello crash handler" in messages
      refute "starting up" in messages
    end

    test "handles file with only crash lines", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            topic: "crash",
            message: "caught signal 11"
          ),
          log_json("2026-03-11T15:22:18Z",
            level: "FATAL",
            topic: "crash",
            message: "Hello crash handler"
          )
        ])

      result = Logs.extract_crash_lines(path)
      assert length(result) == 2
      assert Enum.any?(result, &(&1.message == "caught signal 11"))
      assert Enum.any?(result, &(&1.message == "Hello crash handler"))
    end
  end

  describe "extract_windows/2" do
    test "empty windows list returns []", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-01-15T12:00:00Z", level: "INFO", message: "some line")
        ])

      assert Logs.extract_windows([], path) == []
    end

    test "single window extracts matching entries", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-01-15T12:00:00Z", level: "INFO", message: "before"),
          log_json("2026-01-15T12:00:05Z", level: "INFO", message: "in window"),
          log_json("2026-01-15T12:00:10Z", level: "WARN", message: "also in window"),
          log_json("2026-01-15T12:00:20Z", level: "INFO", message: "after")
        ])

      window_start = ts_us("2026-01-15T12:00:05Z")
      window_end = ts_us("2026-01-15T12:00:10Z")

      [result] = Logs.extract_windows([{window_start, window_end}], path)

      messages = Enum.map(result, & &1.message)
      assert "in window" in messages
      assert "also in window" in messages
      refute "before" in messages
      refute "after" in messages
    end

    test "multiple non-overlapping windows each get their own entries", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-01-15T12:00:00Z", level: "INFO", message: "w1 line1"),
          log_json("2026-01-15T12:00:05Z", level: "INFO", message: "w1 line2"),
          log_json("2026-01-15T12:00:15Z", level: "INFO", message: "gap line"),
          log_json("2026-01-15T12:00:20Z", level: "INFO", message: "w2 line1"),
          log_json("2026-01-15T12:00:25Z", level: "INFO", message: "w2 line2"),
          log_json("2026-01-15T12:00:35Z", level: "INFO", message: "trailing")
        ])

      windows = [
        {ts_us("2026-01-15T12:00:00Z"), ts_us("2026-01-15T12:00:10Z")},
        {ts_us("2026-01-15T12:00:20Z"), ts_us("2026-01-15T12:00:30Z")}
      ]

      [w1, w2] = Logs.extract_windows(windows, path)

      w1_msgs = Enum.map(w1, & &1.message)
      assert "w1 line1" in w1_msgs
      assert "w1 line2" in w1_msgs
      refute "gap line" in w1_msgs

      w2_msgs = Enum.map(w2, & &1.message)
      assert "w2 line1" in w2_msgs
      assert "w2 line2" in w2_msgs
      refute "trailing" in w2_msgs
    end

    test "entry overshooting window 1 appears in window 2", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-01-15T12:00:00Z", level: "INFO", message: "w1 line"),
          log_json("2026-01-15T12:00:10Z", level: "INFO", message: "overshoot into w2"),
          log_json("2026-01-15T12:00:15Z", level: "INFO", message: "w2 line")
        ])

      windows = [
        {ts_us("2026-01-15T12:00:00Z"), ts_us("2026-01-15T12:00:05Z")},
        {ts_us("2026-01-15T12:00:08Z"), ts_us("2026-01-15T12:00:20Z")}
      ]

      [w1, w2] = Logs.extract_windows(windows, path)

      w1_msgs = Enum.map(w1, & &1.message)
      assert "w1 line" in w1_msgs
      refute "overshoot into w2" in w1_msgs

      w2_msgs = Enum.map(w2, & &1.message)
      assert "overshoot into w2" in w2_msgs
      assert "w2 line" in w2_msgs
    end

    test "file not found returns list of empty lists", _context do
      windows = [
        {ts_us("2026-01-15T12:00:00Z"), ts_us("2026-01-15T12:00:05Z")},
        {ts_us("2026-01-15T12:00:10Z"), ts_us("2026-01-15T12:00:15Z")},
        {ts_us("2026-01-15T12:00:20Z"), ts_us("2026-01-15T12:00:25Z")}
      ]

      assert Logs.extract_windows(windows, "/nonexistent/file.log") == [[], [], []]
    end

    test "empty file returns list of empty lists", %{tmp_dir: dir} do
      path = Path.join(dir, "empty.log")
      File.write!(path, "")

      windows = [
        {ts_us("2026-01-15T12:00:00Z"), ts_us("2026-01-15T12:00:05Z")},
        {ts_us("2026-01-15T12:00:10Z"), ts_us("2026-01-15T12:00:15Z")}
      ]

      assert Logs.extract_windows(windows, path) == [[], []]
    end

    test "EOF mid-window finalizes current and pads remaining with empty lists", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-01-15T12:00:00Z", level: "INFO", message: "w1 line"),
          log_json("2026-01-15T12:00:10Z", level: "INFO", message: "w2 partial")
        ])

      windows = [
        {ts_us("2026-01-15T12:00:00Z"), ts_us("2026-01-15T12:00:05Z")},
        {ts_us("2026-01-15T12:00:08Z"), ts_us("2026-01-15T12:00:15Z")},
        {ts_us("2026-01-15T12:00:20Z"), ts_us("2026-01-15T12:00:25Z")}
      ]

      [w1, w2, w3] = Logs.extract_windows(windows, path)

      assert length(w1) == 1
      assert hd(w1).message == "w1 line"
      assert length(w2) == 1
      assert hd(w2).message == "w2 partial"
      assert w3 == []
    end

    test "window with no matching entries returns empty list while others work", %{tmp_dir: dir} do
      path =
        write_log(dir, [
          log_json("2026-01-15T12:00:00Z", level: "INFO", message: "w1 line"),
          log_json("2026-01-15T12:00:05Z", level: "INFO", message: "w1 line2"),
          log_json("2026-01-15T12:00:20Z", level: "INFO", message: "w3 line")
        ])

      windows = [
        {ts_us("2026-01-15T12:00:00Z"), ts_us("2026-01-15T12:00:06Z")},
        {ts_us("2026-01-15T12:00:10Z"), ts_us("2026-01-15T12:00:15Z")},
        {ts_us("2026-01-15T12:00:18Z"), ts_us("2026-01-15T12:00:25Z")}
      ]

      [w1, w2, w3] = Logs.extract_windows(windows, path)

      w1_msgs = Enum.map(w1, & &1.message)
      assert "w1 line" in w1_msgs
      assert "w1 line2" in w1_msgs
      assert w2 == []
      assert length(w3) == 1
      assert hd(w3).message == "w3 line"
    end
  end
end
