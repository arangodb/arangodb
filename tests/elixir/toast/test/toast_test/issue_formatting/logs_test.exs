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

    test "single server returns entries tagged with server id" do
      entries = [
        entry(~U[2026-03-09 10:00:00Z], message: "msg1"),
        entry(~U[2026-03-09 10:00:01Z], message: "msg2")
      ]

      result = Logs.merge_streams([{"coordinator1", entries}])

      assert length(result) == 2
      assert Enum.all?(result, fn {id, _} -> id == "coordinator1" end)
      assert Enum.at(result, 0) |> elem(1) |> Map.get(:message) == "msg1"
      assert Enum.at(result, 1) |> elem(1) |> Map.get(:message) == "msg2"
    end

    test "interleaves multiple servers chronologically" do
      co_entries = [
        entry(~U[2026-03-09 10:00:00Z], message: "co-msg1"),
        entry(~U[2026-03-09 10:00:02Z], message: "co-msg2")
      ]

      db_entries = [
        entry(~U[2026-03-09 10:00:01Z], message: "db-msg1"),
        entry(~U[2026-03-09 10:00:03Z], message: "db-msg2")
      ]

      result =
        Logs.merge_streams([{"coordinator1", co_entries}, {"dbserver1", db_entries}])

      assert Enum.map(result, &elem(&1, 0)) == [
               "coordinator1",
               "dbserver1",
               "coordinator1",
               "dbserver1"
             ]
    end
  end

  # --- server_color/1 ---

  describe "server_color/1" do
    test "coordinator returns a color from the coordinator palette" do
      color = Logs.server_color("coordinator0")
      assert color in [67, 103, 110, 66, 109, 60, 68, 102, 146]
    end

    test "dbserver returns a color from the dbserver palette" do
      color = Logs.server_color("dbserver0")
      assert color in [137, 174, 95, 180, 130, 215, 101, 172, 144]
    end

    test "agent returns a color from the agent palette" do
      color = Logs.server_color("agent0")
      assert color in [101, 138, 66, 144, 96]
    end

    test "different indices return different colors" do
      assert Logs.server_color("coordinator0") != Logs.server_color("coordinator1")
    end

    test "unknown role falls back to coordinator palette" do
      color = Logs.server_color("unknown0")
      assert color in [67, 103, 110, 66, 109, 60, 68, 102, 146]
    end
  end

  # --- format_merged/2 ---

  describe "format_merged/2" do
    test "empty list returns empty string" do
      assert Logs.format_merged([], false) == ""
    end

    test "single server produces untagged lines" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")
      e2 = entry(~U[2026-01-01 00:00:01Z], message: "line2")
      merged = [{"s1", e1}, {"s1", e2}]
      result = Logs.format_merged(merged, false)
      assert result =~ "line1"
      assert result =~ "line2"
    end

    test "multi-server adds server tags" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")
      e2 = entry(~U[2026-01-01 00:00:01Z], message: "line2")
      merged = [{"coordinator1", e1}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, false)
      assert result =~ "[CO1]"
      assert result =~ "[DB1]"
    end

    test "multi-server with color enabled includes ANSI escape sequences" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")
      e2 = entry(~U[2026-01-01 00:00:01Z], message: "line2")
      merged = [{"coordinator1", e1}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, true)
      assert result =~ "\e[38;5;"
    end

    test "WARNING level gets bright emphasis when color enabled" do
      e1 = entry(~U[2026-01-01 00:00:00Z], level: :warning, message: "msg")
      e2 = entry(~U[2026-01-01 00:00:01Z], message: "other")
      merged = [{"coordinator1", e1}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, true)
      assert result =~ IO.ANSI.bright()
    end

    test "ERROR level gets inverse emphasis when color enabled" do
      e1 = entry(~U[2026-01-01 00:00:00Z], level: :error, message: "msg")
      e2 = entry(~U[2026-01-01 00:00:01Z], message: "other")
      merged = [{"coordinator1", e1}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, true)
      assert result =~ IO.ANSI.inverse()
    end
  end

  # --- server_tag/1 with hyphenated IDs ---

  describe "server_tag/1 with hyphenated IDs" do
    test "coordinator with cluster prefix" do
      assert Logs.server_tag("toast-cluster-643-coordinator-0") == "CO0"
    end

    test "dbserver with cluster prefix" do
      assert Logs.server_tag("toast-cluster-643-dbserver-2") == "DB2"
    end

    test "agent with cluster prefix" do
      assert Logs.server_tag("toast-cluster-643-agent-1") == "AG1"
    end
  end

  # --- matching_servers/2 ---

  describe "matching_servers/2" do
    setup do
      servers = %{
        "coordinator1" => %{role: :coordinator},
        "dbserver1" => %{role: :dbserver},
        "agent1" => %{role: :agent}
      }

      %{servers: servers}
    end

    test "with :all filter returns all servers", %{servers: servers} do
      result = Logs.matching_servers(servers, :all)
      assert length(result) == 3
    end

    test "with role filter returns only matching roles", %{servers: servers} do
      result = Logs.matching_servers(servers, [{:role, "coordinator"}])
      assert result == ["coordinator1"]
    end

    test "results are sorted", %{servers: servers} do
      result = Logs.matching_servers(servers, :all)
      assert result == Enum.sort(result)
    end
  end

  # --- extract/3 ---

  describe "extract/3" do
    setup do
      ts = ~U[2026-03-09 10:00:00Z]

      servers = %{
        "coordinator1" => %{
          role: :coordinator,
          arango_id: nil,
          logs: [
            {~U[2026-03-09 09:59:50Z], ~U[2026-03-09 10:00:10Z],
             [
               entry(~U[2026-03-09 09:59:55Z], message: "before"),
               entry(~U[2026-03-09 10:00:00Z], message: "at-time"),
               entry(~U[2026-03-09 10:00:05Z], message: "after")
             ]}
          ]
        },
        "agent1" => %{
          role: :agent,
          arango_id: nil,
          logs: [
            {~U[2026-03-09 09:59:50Z], ~U[2026-03-09 10:00:10Z],
             [entry(~U[2026-03-09 10:00:00Z], message: "agent-msg")]}
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

    test "filters entries by time window", %{servers: servers, window: window} do
      [{_server, entries}] = Logs.extract(servers, window, [{:role, "coordinator"}])

      messages = Enum.map(entries, & &1.message)
      assert "before" in messages
      assert "at-time" in messages
      refute "after" in messages
    end
  end

  # --- extract_events/2 ---

  describe "extract_events/2" do
    test "filters events within the window" do
      events = [
        %{
          event: :server_started,
          timestamp: to_us(~U[2026-03-09 09:59:50Z]),
          server_id: "s1",
          pid: 1
        },
        %{
          event: :test_started,
          timestamp: to_us(~U[2026-03-09 10:00:02Z]),
          module: Mod,
          name: "t"
        },
        %{event: :server_stopped, timestamp: to_us(~U[2026-03-09 10:00:15Z]), server_id: "s1"}
      ]

      window = {~U[2026-03-09 10:00:00Z], ~U[2026-03-09 10:00:10Z]}
      result = Logs.extract_events(events, window)

      assert length(result) == 1
      assert hd(result).event == :test_started
    end

    test "returns empty list when no events match" do
      events = [
        %{
          event: :server_started,
          timestamp: to_us(~U[2026-03-09 09:00:00Z]),
          server_id: "s1",
          pid: 1
        }
      ]

      window = {~U[2026-03-09 10:00:00Z], ~U[2026-03-09 10:00:10Z]}
      assert Logs.extract_events(events, window) == []
    end

    test "includes events at window boundaries" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      events = [%{event: :test_started, timestamp: ts, module: Mod, name: "t"}]
      window = {~U[2026-03-09 10:00:00Z], ~U[2026-03-09 10:00:00Z]}
      assert length(Logs.extract_events(events, window)) == 1
    end
  end

  # --- merge_streams/2 with events ---

  describe "merge_streams/2 with events" do
    test "events interleaved with server logs chronologically" do
      co_entries = [
        entry(~U[2026-03-09 10:00:00Z], message: "co-msg1"),
        entry(~U[2026-03-09 10:00:04Z], message: "co-msg2")
      ]

      events = [
        %{
          event: :test_started,
          timestamp: to_us(~U[2026-03-09 10:00:02Z]),
          module: Mod,
          name: "t"
        }
      ]

      result = Logs.merge_streams([{"coordinator1", co_entries}], events)

      assert Enum.map(result, &elem(&1, 0)) == ["coordinator1", :event, "coordinator1"]
    end

    test "events only (no server entries)" do
      events = [
        %{
          event: :test_started,
          timestamp: to_us(~U[2026-03-09 10:00:00Z]),
          module: Mod,
          name: "t"
        }
      ]

      result = Logs.merge_streams([], events)
      assert length(result) == 1
      assert {:event, _} = hd(result)
    end

    test "empty events preserves original behavior" do
      entries = [entry(~U[2026-03-09 10:00:00Z], message: "msg")]
      result = Logs.merge_streams([{"s1", entries}], [])
      assert length(result) == 1
      assert {"s1", _} = hd(result)
    end
  end

  # --- format_event/1 ---

  describe "format_event/1" do
    test "server_started" do
      event = %{event: :server_started, server_id: "dbserver1", pid: 12345, timestamp: 0}
      assert Logs.format_event(event) == ">>> server_started dbserver1 (pid=12345)"
    end

    test "server_stopped" do
      event = %{event: :server_stopped, server_id: "dbserver1", timestamp: 0}
      assert Logs.format_event(event) == ">>> server_stopped dbserver1"
    end

    test "server_crashed" do
      event = %{
        event: :server_crashed,
        server_id: "dbserver1",
        pid: 123,
        signal: 11,
        timestamp: 0
      }

      assert Logs.format_event(event) == ">>> server_crashed dbserver1 (pid=123, signal=11)"
    end

    test "server_killed" do
      event = %{event: :server_killed, server_id: "s1", timestamp: 0}
      assert Logs.format_event(event) == ">>> server_killed s1"
    end

    test "server_paused" do
      event = %{event: :server_paused, server_id: "s1", timestamp: 0}
      assert Logs.format_event(event) == ">>> server_paused s1"
    end

    test "server_resumed" do
      event = %{event: :server_resumed, server_id: "s1", timestamp: 0}
      assert Logs.format_event(event) == ">>> server_resumed s1"
    end

    test "test_started" do
      event = %{event: :test_started, module: MyModule, name: "my test", timestamp: 0}
      assert Logs.format_event(event) == ">>> test_started MyModule > my test"
    end

    test "test_finished" do
      event = %{
        event: :test_finished,
        module: MyModule,
        name: "my test",
        outcome: :passed,
        timestamp: 0
      }

      assert Logs.format_event(event) == ">>> test_finished MyModule > my test (passed)"
    end

    test "module_started" do
      event = %{event: :module_started, module: MyModule, timestamp: 0}
      assert Logs.format_event(event) == ">>> module_started MyModule"
    end

    test "module_finished" do
      event = %{event: :module_finished, module: MyModule, timestamp: 0}
      assert Logs.format_event(event) == ">>> module_finished MyModule"
    end

    test "deployment_starting" do
      event = %{event: :deployment_starting, deployment_id: "d1", mode: :cluster, timestamp: 0}
      assert Logs.format_event(event) == ">>> deployment_starting d1 (cluster)"
    end

    test "deployment_started" do
      event = %{event: :deployment_started, deployment_id: "d1", timestamp: 0}
      assert Logs.format_event(event) == ">>> deployment_started d1"
    end

    test "deployment_stopped" do
      event = %{event: :deployment_stopped, deployment_id: "d1", timestamp: 0}
      assert Logs.format_event(event) == ">>> deployment_stopped d1"
    end

    test "timeout_kill" do
      event = %{event: :timeout_kill, reason: "test exceeded 60s", timestamp: 0}
      assert Logs.format_event(event) == ">>> timeout_kill test exceeded 60s"
    end

    test "server_identified" do
      event = %{event: :server_identified, server_id: "s1", arango_id: "CRDN-abc", timestamp: 0}
      assert Logs.format_event(event) == ">>> server_identified s1 => CRDN-abc"
    end

    test "unknown event" do
      event = %{event: :something_new, timestamp: 0}
      assert Logs.format_event(event) == ">>> something_new"
    end
  end

  # --- format_merged/2 with events ---

  describe "format_merged/2 with events" do
    test "single server: events render with >>> prefix, no tag" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")

      event = %{
        event: :test_started,
        timestamp: to_us(~U[2026-01-01 00:00:01Z]),
        module: Mod,
        name: "t"
      }

      merged = [{"s1", e1}, {:event, event}]
      result = Logs.format_merged(merged, false)

      assert result =~ "line1"
      assert result =~ ">>> test_started"
      refute result =~ "[" <> "s1" <> "]"
    end

    test "multi-server: events render without server color" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")
      e2 = entry(~U[2026-01-01 00:00:02Z], message: "line2")

      event = %{
        event: :server_crashed,
        timestamp: to_us(~U[2026-01-01 00:00:01Z]),
        server_id: "db1",
        pid: 1,
        signal: 11
      }

      merged = [{"coordinator1", e1}, {:event, event}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, true)

      lines = String.split(result, "\n")
      event_line = Enum.find(lines, &String.contains?(&1, ">>>"))

      assert event_line != nil
      # Event line should NOT have server color escape
      refute event_line =~ "\e[38;5;"
      assert event_line =~ ">>> server_crashed"
    end

    test "multi-server: event lines are padded to align with tags" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")
      e2 = entry(~U[2026-01-01 00:00:02Z], message: "line2")

      event = %{
        event: :test_started,
        timestamp: to_us(~U[2026-01-01 00:00:01Z]),
        module: Mod,
        name: "t"
      }

      merged = [{"coordinator1", e1}, {:event, event}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, false)

      lines = String.split(result, "\n")
      tag_line = Enum.find(lines, &String.contains?(&1, "[CO1]"))
      event_line = Enum.find(lines, &String.contains?(&1, ">>>"))

      # Both lines should have content starting at similar column
      # Tag line: "[CO1] ..." event line: "      ..."
      assert event_line =~ ">>> test_started"
      assert String.starts_with?(tag_line, "[CO1")
    end

    test "events only renders without tags" do
      event = %{
        event: :test_started,
        timestamp: to_us(~U[2026-01-01 00:00:00Z]),
        module: Mod,
        name: "t"
      }

      result = Logs.format_merged([{:event, event}], false)
      assert result =~ ">>> test_started"
    end

    test "full detail includes inspect of the event map" do
      event = %{
        event: :server_started,
        timestamp: to_us(~U[2026-01-01 00:00:00Z]),
        server_id: "db1",
        pid: 123
      }

      result = Logs.format_merged([{:event, event}], false, :full)
      assert result =~ ">>> server_started db1 (pid=123)"
      assert result =~ "server_id: \"db1\""
      assert result =~ "pid: 123"
    end

    test "basic detail does not include inspect output" do
      event = %{
        event: :server_started,
        timestamp: to_us(~U[2026-01-01 00:00:00Z]),
        server_id: "db1",
        pid: 123
      }

      result = Logs.format_merged([{:event, event}], false, :basic)
      assert result =~ ">>> server_started db1 (pid=123)"
      refute result =~ "server_id: \"db1\""
    end

    test "multi-server full detail includes inspect after event line" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")

      event = %{
        event: :server_started,
        timestamp: to_us(~U[2026-01-01 00:00:01Z]),
        server_id: "db1",
        pid: 123
      }

      merged = [{"coordinator1", e1}, {:event, event}]
      result = Logs.format_merged(merged, false, :full)
      assert result =~ ">>> server_started db1 (pid=123)"
      assert result =~ "server_id: \"db1\""
    end
  end

  # --- Helpers ---

  defp to_us(%DateTime{} = dt), do: DateTime.to_unix(dt, :microsecond)

  defp entry(time, opts \\ []) do
    %{
      time: DateTime.to_unix(time, :microsecond),
      message: Keyword.get(opts, :message, "msg"),
      level: Keyword.get(opts, :level, :info)
    }
    |> maybe_put(:topic, opts[:topic])
    |> maybe_put(:id, opts[:id])
    |> maybe_put(:pid, opts[:pid])
  end

  defp maybe_put(map, _key, nil), do: map
  defp maybe_put(map, key, val), do: Map.put(map, key, val)
end
