defmodule ToastTest.IssueFormatting.LogsTest do
  use ExUnit.Case, async: true

  alias ToastTest.IssueFormatting.Logs

  # --- parse_server_filter/1 ---

  describe "parse_server_filter/1" do
    test "nil returns default (exclude agents)" do
      result = Logs.parse_server_filter(nil)
      assert {:role, "coordinator"} in result
      assert {:role, "dbserver"} in result
      assert {:role, "single"} in result

      refute Enum.any?(result, fn
               {:role, r} -> r == "agent"
               _ -> false
             end)
    end

    test "\"all\" returns :all" do
      assert Logs.parse_server_filter("all") == :all
    end

    test "known roles become :role filters" do
      assert Logs.parse_server_filter("coordinator,dbserver") == [
               {:role, "coordinator"},
               {:role, "dbserver"}
             ]
    end

    test "unknown strings become :prefix filters" do
      assert Logs.parse_server_filter("coordinator1,agent1") == [
               {:prefix, "coordinator1"},
               {:prefix, "agent1"}
             ]
    end

    test "mixed roles and prefixes" do
      assert Logs.parse_server_filter("coordinator,agent1") == [
               {:role, "coordinator"},
               {:prefix, "agent1"}
             ]
    end
  end

  # --- parse_window_spec/1 ---

  describe "parse_window_spec/1" do
    test "nil returns nil" do
      assert Logs.parse_window_spec(nil) == nil
    end

    test "single number" do
      assert Logs.parse_window_spec("30") == {30, 0}
    end

    test "before,after" do
      assert Logs.parse_window_spec("30,5") == {30, 5}
    end
  end

  # --- server_matches?/2 ---

  describe "server_matches?/2" do
    test ":all matches everything" do
      assert Logs.server_matches?("coordinator1", :all)
      assert Logs.server_matches?("agent1", :all)
    end

    test "role matching" do
      filter = [{:role, "coordinator"}]
      assert Logs.server_matches?("coordinator1", filter)
      assert Logs.server_matches?("coordinator2", filter)
      refute Logs.server_matches?("dbserver1", filter)
    end

    test "prefix matching" do
      filter = [{:prefix, "coordinator1"}]
      assert Logs.server_matches?("coordinator1", filter)
      refute Logs.server_matches?("coordinator2", filter)
    end

    test "multiple filters compose as union" do
      filter = [{:role, "coordinator"}, {:prefix, "agent1"}]
      assert Logs.server_matches?("coordinator1", filter)
      assert Logs.server_matches?("agent1", filter)
      refute Logs.server_matches?("dbserver1", filter)
      refute Logs.server_matches?("agent2", filter)
    end
  end

  # --- display_window/2 ---

  describe "display_window/2" do
    test "nil time_bounds returns nil" do
      issue = %{type: :crash, time_bounds: nil}
      assert Logs.display_window(issue, nil) == nil
    end

    test "crash default window" do
      ts = ~U[2026-03-09 10:00:00Z]
      issue = %{type: :crash, time_bounds: {ts, ts}}
      {start_dt, end_dt} = Logs.display_window(issue, nil)
      assert DateTime.diff(ts, start_dt) == 20
      assert DateTime.diff(end_dt, ts) == 0
    end

    test "test_failure default window" do
      s = ~U[2026-03-09 10:00:00Z]
      f = ~U[2026-03-09 10:00:05Z]
      issue = %{type: :test_failure, time_bounds: {s, f}}
      {start_dt, end_dt} = Logs.display_window(issue, nil)
      assert DateTime.diff(s, start_dt) == 1
      assert DateTime.diff(end_dt, f) == 1
    end

    test "timeout default window" do
      ts = ~U[2026-03-09 10:00:00Z]
      issue = %{type: :timeout, time_bounds: {ts, ts}}
      {start_dt, end_dt} = Logs.display_window(issue, nil)
      assert DateTime.diff(ts, start_dt) == 10
      assert DateTime.diff(end_dt, ts) == 0
    end

    test "sanitizer_report default window" do
      ts = ~U[2026-03-09 10:00:00Z]
      issue = %{type: :sanitizer_report, time_bounds: {ts, ts}}
      {start_dt, end_dt} = Logs.display_window(issue, nil)
      assert DateTime.diff(ts, start_dt) == 5
      assert DateTime.diff(end_dt, ts) == 1
    end

    test "custom window spec overrides defaults" do
      ts = ~U[2026-03-09 10:00:00Z]
      issue = %{type: :crash, time_bounds: {ts, ts}}
      {start_dt, end_dt} = Logs.display_window(issue, {-30, 10})
      assert DateTime.diff(ts, start_dt) == 30
      assert DateTime.diff(end_dt, ts) == 10
    end
  end

  # --- filter_lines/2 ---

  describe "filter_lines/2" do
    test "includes lines within window" do
      lines = """
      2026-03-09T10:00:00Z [1] INFO msg1
      2026-03-09T10:00:01Z [1] INFO msg2
      2026-03-09T10:00:02Z [1] INFO msg3\
      """

      result = Logs.filter_lines(lines, {"2026-03-09T10:00:00Z", "2026-03-09T10:00:01Z"})
      assert result =~ "msg1"
      assert result =~ "msg2"
      refute result =~ "msg3"
    end

    test "includes continuation lines when preceding line is included" do
      lines = """
      2026-03-09T10:00:00Z [1] INFO msg1
        continuation line
      2026-03-09T10:00:02Z [1] INFO msg2\
      """

      result = Logs.filter_lines(lines, {"2026-03-09T10:00:00Z", "2026-03-09T10:00:01Z"})
      assert result =~ "msg1"
      assert result =~ "continuation line"
      refute result =~ "msg2"
    end

    test "excludes continuation lines when preceding line is excluded" do
      lines = """
      2026-03-09T10:00:00Z [1] INFO msg1
        continuation line
      2026-03-09T10:00:02Z [1] INFO msg2\
      """

      result = Logs.filter_lines(lines, {"2026-03-09T10:00:01Z", "2026-03-09T10:00:03Z"})
      refute result =~ "msg1"
      refute result =~ "continuation line"
      assert result =~ "msg2"
    end

    test "empty input" do
      assert Logs.filter_lines("", {"2026-03-09T10:00:00Z", "2026-03-09T10:00:01Z"}) == ""
    end
  end

  # --- server_tag/1 ---

  describe "server_tag/1" do
    test "coordinator" do
      assert Logs.server_tag("coordinator1") == "CO1"
      assert Logs.server_tag("coordinator2") == "CO2"
    end

    test "dbserver" do
      assert Logs.server_tag("dbserver1") == "DB1"
      assert Logs.server_tag("dbserver2") == "DB2"
    end

    test "agent" do
      assert Logs.server_tag("agent1") == "AG1"
    end

    test "single" do
      assert Logs.server_tag("single") == "SNG"
    end
  end

  # --- merge_streams/1 ---

  describe "merge_streams/1" do
    test "empty" do
      assert Logs.merge_streams([]) == []
    end

    test "single server returns lines as-is" do
      lines = "2026-03-09T10:00:00Z [1] INFO msg1\n2026-03-09T10:00:01Z [1] INFO msg2"
      result = Logs.merge_streams([{"coordinator1", lines}])

      assert result == [
               {"coordinator1", "2026-03-09T10:00:00Z [1] INFO msg1"},
               {"coordinator1", "2026-03-09T10:00:01Z [1] INFO msg2"}
             ]
    end

    test "interleaves multiple servers chronologically" do
      co_lines = "2026-03-09T10:00:00Z [1] INFO co-msg1\n2026-03-09T10:00:02Z [1] INFO co-msg2"
      db_lines = "2026-03-09T10:00:01Z [2] INFO db-msg1\n2026-03-09T10:00:03Z [2] INFO db-msg2"

      result = Logs.merge_streams([{"coordinator1", co_lines}, {"dbserver1", db_lines}])

      assert Enum.map(result, &elem(&1, 0)) == [
               "coordinator1",
               "dbserver1",
               "coordinator1",
               "dbserver1"
             ]
    end

    test "continuation lines stay with their preceding timestamped line" do
      co_lines =
        "2026-03-09T10:00:00Z [1] INFO co-msg\n  co-continuation"

      db_lines = "2026-03-09T10:00:00Z [2] INFO db-msg"

      result = Logs.merge_streams([{"coordinator1", co_lines}, {"dbserver1", db_lines}])

      # Both have same timestamp — order depends on input order, but continuation stays with co
      co_entries = Enum.filter(result, fn {id, _} -> id == "coordinator1" end)
      assert length(co_entries) == 2
      assert Enum.at(co_entries, 1) == {"coordinator1", "  co-continuation"}
    end
  end

  # --- extract/4 ---

  describe "extract/3" do
    setup do
      ts = ~U[2026-03-09 10:00:00Z]

      servers = %{
        "coordinator1" => %{
          role: :coordinator,
          arango_id: nil,
          logs: [
            {~U[2026-03-09 09:59:50Z], ~U[2026-03-09 10:00:10Z],
             "2026-03-09T09:59:55Z [1] INFO before\n2026-03-09T10:00:00Z [1] INFO at-time\n2026-03-09T10:00:05Z [1] INFO after"}
          ]
        },
        "agent1" => %{
          role: :agent,
          arango_id: nil,
          logs: [
            {~U[2026-03-09 09:59:50Z], ~U[2026-03-09 10:00:10Z],
             "2026-03-09T10:00:00Z [2] INFO agent-msg"}
          ]
        }
      }

      # Crash default: 20s before, 0s after
      issue = %{type: :crash, time_bounds: {ts, ts}}
      window = Logs.display_window(issue, nil)

      %{servers: servers, window: window, ts: ts}
    end

    test "default filter excludes agents", %{servers: servers, window: window} do
      filter = Logs.parse_server_filter(nil)
      result = Logs.extract(servers, window, filter)
      server_ids = Enum.map(result, &elem(&1, 0))
      assert "coordinator1" in server_ids
      refute "agent1" in server_ids
    end

    test ":all filter includes agents", %{servers: servers, window: window} do
      result = Logs.extract(servers, window, :all)
      server_ids = Enum.map(result, &elem(&1, 0))
      assert "agent1" in server_ids
    end

    test "filters lines by time window", %{servers: servers, window: window} do
      [{_server, lines}] = Logs.extract(servers, window, [{:role, "coordinator"}])

      assert lines =~ "before"
      assert lines =~ "at-time"
      refute lines =~ "after"
    end
  end
end
