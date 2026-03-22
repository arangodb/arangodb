defmodule ToastTest.Attribution.ServerLogsTest do
  use ExUnit.Case, async: true

  alias ToastTest.Attribution.ServerLogs

  @base_date ~D[2026-01-15]

  defp dt(time_str) do
    DateTime.new!(@base_date, Time.from_iso8601!(time_str))
  end

  defp windows(tests \\ %{}) do
    %{
      suite: %{started_at: dt("10:00:00"), finished_at: dt("10:10:00")},
      modules: %{},
      tests: tests
    }
  end

  # --- compute_windows/2 ---

  describe "compute_windows/2 — test_failure" do
    test "pads test window by [-1s, +1s]" do
      test_windows =
        windows(%{
          {MyMod, :test_one} => %{started_at: dt("10:01:00"), finished_at: dt("10:02:00")}
        })

      issue = %{
        type: :test_failure,
        scope: {:test, MyMod, :test_one},
        detail: %{}
      }

      [{start, finish}] = ServerLogs.compute_windows([issue], test_windows)

      assert start == dt("10:00:59")
      assert finish == dt("10:02:01")
    end

    test "produces no window for suite-scoped test_failure" do
      issue = %{type: :test_failure, scope: :suite, detail: %{}}

      assert ServerLogs.compute_windows([issue], windows()) == []
    end

    test "produces no window for module-scoped test_failure" do
      issue = %{type: :test_failure, scope: {:module, MyMod}, detail: %{}}

      assert ServerLogs.compute_windows([issue], windows()) == []
    end

    test "produces no window when test is not in windows map" do
      issue = %{
        type: :test_failure,
        scope: {:test, MyMod, :nonexistent_test},
        detail: %{}
      }

      assert ServerLogs.compute_windows([issue], windows()) == []
    end
  end

  describe "compute_windows/2 — sanitizer_report" do
    test "pads timestamp by [-5s, +1s]" do
      ts = dt("10:03:00")

      issue = %{
        type: :sanitizer_report,
        detail: %{report: "some report", timestamp: ts}
      }

      [{start, finish}] = ServerLogs.compute_windows([issue], windows())

      assert start == dt("10:02:55")
      assert finish == dt("10:03:01")
    end

    test "produces no window when detail lacks timestamp" do
      issue = %{
        type: :sanitizer_report,
        detail: %{report: "some report"}
      }

      assert ServerLogs.compute_windows([issue], windows()) == []
    end

    test "produces no window when timestamp is nil" do
      issue = %{
        type: :sanitizer_report,
        detail: %{report: "some report", timestamp: nil}
      }

      assert ServerLogs.compute_windows([issue], windows()) == []
    end
  end

  describe "compute_windows/2 — crash" do
    test "pads crash timestamp by [-20s, 0s]" do
      ts = dt("10:05:00")

      issue = %{
        type: :crash,
        detail: %{crash_info: %{timestamp: ts}}
      }

      [{start, finish}] = ServerLogs.compute_windows([issue], windows())

      assert start == dt("10:04:40")
      assert finish == dt("10:05:00")
    end

    test "produces no window when crash_info has nil timestamp" do
      issue = %{
        type: :crash,
        detail: %{crash_info: %{timestamp: nil}}
      }

      assert ServerLogs.compute_windows([issue], windows()) == []
    end

    test "produces no window when crash_info is missing timestamp key" do
      issue = %{
        type: :crash,
        detail: %{crash_info: %{}}
      }

      assert ServerLogs.compute_windows([issue], windows()) == []
    end

    test "produces no window when detail lacks crash_info" do
      issue = %{type: :crash, detail: %{}}

      assert ServerLogs.compute_windows([issue], windows()) == []
    end
  end

  describe "compute_windows/2 — timeout" do
    test "pads timeout timestamp by [-10s, 0s]" do
      ts = dt("10:05:00")
      issue = %{type: :timeout, detail: %{timestamp: ts}}

      [{start, finish}] = ServerLogs.compute_windows([issue], windows())

      assert start == dt("10:04:50")
      assert finish == dt("10:05:00")
    end

    test "produces no window when detail lacks timestamp" do
      issue = %{type: :timeout, detail: %{}}

      assert ServerLogs.compute_windows([issue], windows()) == []
    end

    test "produces no window when timestamp is nil" do
      issue = %{type: :timeout, detail: %{timestamp: nil}}

      assert ServerLogs.compute_windows([issue], windows()) == []
    end
  end

  describe "compute_windows/2 — unknown type" do
    test "unknown issue type produces no window" do
      issue = %{type: :unknown, detail: %{}}

      assert ServerLogs.compute_windows([issue], windows()) == []
    end
  end

  describe "compute_windows/2 — multiple issues" do
    test "collects windows from mixed issue types" do
      crash_ts = dt("10:05:00")
      timeout_ts = dt("10:08:00")

      issues = [
        %{type: :crash, detail: %{crash_info: %{timestamp: crash_ts}}},
        %{type: :timeout, detail: %{timestamp: timeout_ts}},
        %{type: :crash, detail: %{}}
      ]

      result = ServerLogs.compute_windows(issues, windows())

      assert length(result) == 2
    end
  end

  # --- merge_windows/1 ---

  describe "merge_windows/1" do
    test "empty list returns empty" do
      assert ServerLogs.merge_windows([]) == []
    end

    test "single window passes through" do
      w = {dt("10:00:00"), dt("10:01:00")}

      assert ServerLogs.merge_windows([w]) == [w]
    end

    test "non-overlapping windows stay separate" do
      w1 = {dt("10:00:00"), dt("10:01:00")}
      w2 = {dt("10:02:00"), dt("10:03:00")}

      assert ServerLogs.merge_windows([w1, w2]) == [w1, w2]
    end

    test "overlapping windows are merged" do
      w1 = {dt("10:00:00"), dt("10:02:00")}
      w2 = {dt("10:01:00"), dt("10:03:00")}

      assert ServerLogs.merge_windows([w1, w2]) == [{dt("10:00:00"), dt("10:03:00")}]
    end

    test "adjacent windows (touching boundaries) are merged" do
      w1 = {dt("10:00:00"), dt("10:01:00")}
      w2 = {dt("10:01:00"), dt("10:02:00")}

      assert ServerLogs.merge_windows([w1, w2]) == [{dt("10:00:00"), dt("10:02:00")}]
    end

    test "multiple overlapping windows merge into one" do
      w1 = {dt("10:00:00"), dt("10:02:00")}
      w2 = {dt("10:01:00"), dt("10:03:00")}
      w3 = {dt("10:02:30"), dt("10:04:00")}

      assert ServerLogs.merge_windows([w1, w2, w3]) == [{dt("10:00:00"), dt("10:04:00")}]
    end

    test "out-of-order input is sorted before merging" do
      w1 = {dt("10:05:00"), dt("10:06:00")}
      w2 = {dt("10:00:00"), dt("10:01:00")}

      assert ServerLogs.merge_windows([w1, w2]) == [
               {dt("10:00:00"), dt("10:01:00")},
               {dt("10:05:00"), dt("10:06:00")}
             ]
    end

    test "out-of-order overlapping windows are sorted and merged" do
      w1 = {dt("10:02:00"), dt("10:04:00")}
      w2 = {dt("10:00:00"), dt("10:03:00")}

      assert ServerLogs.merge_windows([w1, w2]) == [{dt("10:00:00"), dt("10:04:00")}]
    end

    test "window fully contained within another is absorbed" do
      outer = {dt("10:00:00"), dt("10:05:00")}
      inner = {dt("10:01:00"), dt("10:02:00")}

      assert ServerLogs.merge_windows([outer, inner]) == [outer]
    end

    test "mix of overlapping and non-overlapping" do
      w1 = {dt("10:00:00"), dt("10:02:00")}
      w2 = {dt("10:01:00"), dt("10:03:00")}
      w3 = {dt("10:05:00"), dt("10:06:00")}

      assert ServerLogs.merge_windows([w1, w2, w3]) == [
               {dt("10:00:00"), dt("10:03:00")},
               {dt("10:05:00"), dt("10:06:00")}
             ]
    end
  end

  # --- collect/3 ---

  describe "collect/3" do
    @tmp_dir Path.join(
               System.tmp_dir!(),
               "toast_server_logs_test_#{System.unique_integer([:positive])}"
             )

    setup do
      File.mkdir_p!(@tmp_dir)
      on_exit(fn -> File.rm_rf!(@tmp_dir) end)
      {:ok, tmp_dir: @tmp_dir}
    end

    defp log_line(time_str, msg) do
      ~s({"time":"2026-01-15T#{time_str}Z","level":"INFO","pid":"12345","id":"abc12","topic":"general","message":"#{msg}"}\n)
    end

    defp write_log(dir, name, lines) do
      path = Path.join(dir, name)
      File.write!(path, Enum.join(lines))
      path
    end

    defp make_log_files(servers) do
      for {id, log_file} <- servers, log_file != nil, into: %{} do
        {id, log_file}
      end
    end

    test "empty issues produce empty server_logs", %{tmp_dir: dir} do
      log_path = write_log(dir, "server.log", [log_line("10:00:05", "hello")])
      log_files = make_log_files([{"agent1", log_path}])

      assert ServerLogs.collect([], log_files, windows()) == %{}
    end

    test "extracts log lines within crash time window", %{tmp_dir: dir} do
      lines = [
        log_line("10:04:30", "before window"),
        log_line("10:04:45", "inside window early"),
        log_line("10:04:55", "inside window late"),
        log_line("10:05:00", "at crash time"),
        log_line("10:05:05", "after window")
      ]

      log_path = write_log(dir, "agent.log", lines)
      log_files = make_log_files([{"agent1", log_path}])

      crash_ts = dt("10:05:00")
      issues = [%{type: :crash, detail: %{crash_info: %{timestamp: crash_ts}}}]

      result = ServerLogs.collect(issues, log_files, windows())

      assert map_size(result) == 1
      [{start, finish, entries}] = result["agent1"]

      # Crash window: [-20s, 0s] => 10:04:40 - 10:05:00
      assert start == dt("10:04:40")
      assert finish == dt("10:05:00")
      messages = Enum.map(entries, & &1.message)
      assert "inside window early" in messages
      assert "inside window late" in messages
      assert "at crash time" in messages
      refute "before window" in messages
      refute "after window" in messages
    end

    test "servers with nil log_file are excluded from log_files map", %{tmp_dir: _dir} do
      log_files = make_log_files([{"agent1", nil}])

      assert log_files == %{}
    end

    test "servers with no matching lines produce empty excerpt list", %{tmp_dir: dir} do
      # Log file has lines outside any window
      lines = [
        log_line("09:00:00", "way before"),
        log_line("11:00:00", "way after")
      ]

      log_path = write_log(dir, "agent.log", lines)
      log_files = make_log_files([{"agent1", log_path}])

      issues = [%{type: :timeout, detail: %{timestamp: dt("10:05:00")}}]
      result = ServerLogs.collect(issues, log_files, windows())

      assert result["agent1"] == []
    end

    test "multiple servers each get their own excerpts", %{tmp_dir: dir} do
      lines1 = [
        log_line("10:04:55", "agent1 line")
      ]

      lines2 = [
        log_line("10:04:55", "dbserver1 line")
      ]

      log1 = write_log(dir, "agent1.log", lines1)
      log2 = write_log(dir, "dbserver1.log", lines2)
      log_files = make_log_files([{"agent1", log1}, {"dbserver1", log2}])

      issues = [%{type: :crash, detail: %{crash_info: %{timestamp: dt("10:05:00")}}}]
      result = ServerLogs.collect(issues, log_files, windows())

      assert map_size(result) == 2
      [{_, _, entries1}] = result["agent1"]
      [{_, _, entries2}] = result["dbserver1"]
      assert Enum.any?(entries1, &(&1.message == "agent1 line"))
      assert Enum.any?(entries2, &(&1.message == "dbserver1 line"))
    end

    test "merged windows produce separate excerpts per window", %{tmp_dir: dir} do
      lines = [
        log_line("10:02:52", "in timeout window"),
        log_line("10:03:05", "between windows"),
        log_line("10:04:45", "in crash window")
      ]

      log_path = write_log(dir, "agent.log", lines)
      log_files = make_log_files([{"agent1", log_path}])

      # Two non-overlapping issues:
      # timeout at 10:03:00 => window [10:02:50, 10:03:00]
      # crash at 10:05:00 => window [10:04:40, 10:05:00]
      issues = [
        %{type: :timeout, detail: %{timestamp: dt("10:03:00")}},
        %{type: :crash, detail: %{crash_info: %{timestamp: dt("10:05:00")}}}
      ]

      result = ServerLogs.collect(issues, log_files, windows())
      excerpts = result["agent1"]

      assert length(excerpts) == 2

      all_messages =
        Enum.flat_map(excerpts, fn {_, _, entries} -> Enum.map(entries, & &1.message) end)

      assert "in timeout window" in all_messages
      assert "in crash window" in all_messages
      refute "between windows" in all_messages
    end
  end
end
