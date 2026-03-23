defmodule ToastTest.IssueFormatting.LogsTest do
  use ExUnit.Case, async: true

  alias ToastTest.IssueFormatting.Logs

  @usec_per_sec 1_000_000

  # --- parse_server_filter/1 ---

  describe "parse_server_filter/1" do
    test "nil returns default (exclude agents)" do
      result = Logs.parse_server_filter(nil)
      assert {:role, :coordinator} in result
      assert {:role, :dbserver} in result
      assert {:role, :single} in result

      refute Enum.any?(result, fn
               {:role, r} -> r == :agent
               _ -> false
             end)
    end

    test "\"all\" returns :all" do
      assert Logs.parse_server_filter("all") == :all
    end

    test "known roles become :role filters" do
      assert Logs.parse_server_filter("coordinator,dbserver") == [
               {:role, :coordinator},
               {:role, :dbserver}
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
               {:role, :coordinator},
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

  # --- parse_exclude/1 ---

  describe "parse_exclude/1" do
    test "nil returns nil" do
      assert Logs.parse_exclude(nil) == nil
    end

    test "single ID" do
      assert Logs.parse_exclude("e6460") == MapSet.new(["e6460"])
    end

    test "comma-separated IDs" do
      result = Logs.parse_exclude("e6460,b387d,abc12")
      assert result == MapSet.new(["e6460", "b387d", "abc12"])
    end

    test "trims whitespace" do
      result = Logs.parse_exclude(" e6460 , b387d ")
      assert result == MapSet.new(["e6460", "b387d"])
    end
  end

  # --- server_matches?/3 ---

  describe "server_matches?/3" do
    test ":all matches everything" do
      assert Logs.server_matches?("coordinator1", :all, :coordinator)
      assert Logs.server_matches?("agent1", :all, :agent)
    end

    test "role matching" do
      filter = [{:role, :coordinator}]
      assert Logs.server_matches?("coordinator1", filter, :coordinator)
      assert Logs.server_matches?("coordinator2", filter, :coordinator)
      refute Logs.server_matches?("dbserver1", filter, :dbserver)
    end

    test "prefix matching" do
      filter = [{:prefix, "coordinator1"}]
      assert Logs.server_matches?("coordinator1", filter, :coordinator)
      refute Logs.server_matches?("coordinator2", filter, :coordinator)
    end

    test "multiple filters compose as union" do
      filter = [{:role, :coordinator}, {:prefix, "agent1"}]
      assert Logs.server_matches?("coordinator1", filter, :coordinator)
      assert Logs.server_matches?("agent1", filter, :agent)
      refute Logs.server_matches?("dbserver1", filter, :dbserver)
      refute Logs.server_matches?("agent2", filter, :agent)
    end

    test "role from metadata takes precedence over server ID" do
      filter = [{:role, :single}]
      assert Logs.server_matches?("single-00", filter, :single)
    end
  end

  # --- display_window/2 ---

  describe "display_window/2" do
    test "nil time_bounds returns nil" do
      issue = %{type: :crash, time_bounds: nil}
      assert Logs.display_window(issue, nil) == nil
    end

    test "crash default window" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      issue = %{type: :crash, time_bounds: {ts, ts}}
      {start_us, end_us} = Logs.display_window(issue, nil)
      assert ts - start_us == 20 * @usec_per_sec
      assert end_us - ts == 0
    end

    test "test_failure default window" do
      s = to_us(~U[2026-03-09 10:00:00Z])
      f = to_us(~U[2026-03-09 10:00:05Z])
      issue = %{type: :test_failure, time_bounds: {s, f}}
      {start_us, end_us} = Logs.display_window(issue, nil)
      assert s - start_us == 1 * @usec_per_sec
      assert end_us - f == 1 * @usec_per_sec
    end

    test "timeout default window" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      issue = %{type: :timeout, time_bounds: {ts, ts}}
      {start_us, end_us} = Logs.display_window(issue, nil)
      assert ts - start_us == 10 * @usec_per_sec
      assert end_us - ts == 0
    end

    test "sanitizer_report default window" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      issue = %{type: :sanitizer_report, time_bounds: {ts, ts}}
      {start_us, end_us} = Logs.display_window(issue, nil)
      assert ts - start_us == 5 * @usec_per_sec
      assert end_us - ts == 1 * @usec_per_sec
    end

    test "custom window spec overrides defaults" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      issue = %{type: :crash, time_bounds: {ts, ts}}
      {start_us, end_us} = Logs.display_window(issue, {-30_000, 10_000})
      assert ts - start_us == 30 * @usec_per_sec
      assert end_us - ts == 10 * @usec_per_sec
    end
  end

  # --- server_tag/2 ---

  describe "server_tag/2" do
    test "coordinator" do
      assert Logs.server_tag("coordinator1", :coordinator) == "CO1"
      assert Logs.server_tag("coordinator2", :coordinator) == "CO2"
    end

    test "dbserver" do
      assert Logs.server_tag("dbserver1", :dbserver) == "DB1"
      assert Logs.server_tag("dbserver2", :dbserver) == "DB2"
    end

    test "agent" do
      assert Logs.server_tag("agent1", :agent) == "AG1"
    end

    test "single" do
      assert Logs.server_tag("single", :single) == "SNG"
    end

    test "unconventional name with role from metadata" do
      assert Logs.server_tag("single-00", :single) == "SNG00"
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

  # --- server_color/2 ---

  describe "server_color/2" do
    test "coordinator returns a color from the coordinator palette" do
      assert Logs.server_color(:coordinator, 0) in [67, 103, 110, 66, 109, 60, 68, 102, 146]
    end

    test "dbserver returns a color from the dbserver palette" do
      assert Logs.server_color(:dbserver, 0) in [137, 174, 95, 180, 130, 215, 101, 172, 144]
    end

    test "agent returns a color from the agent palette" do
      assert Logs.server_color(:agent, 0) in [101, 138, 66, 144, 96]
    end

    test "different indices return different colors" do
      assert Logs.server_color(:coordinator, 0) != Logs.server_color(:coordinator, 1)
    end

    test "single role uses dbserver palette" do
      assert Logs.server_color(:single, 0) in [137, 174, 95, 180, 130, 215, 101, 172, 144]
    end

    test "unknown role falls back to coordinator palette" do
      assert Logs.server_color(:unknown, 0) in [67, 103, 110, 66, 109, 60, 68, 102, 146]
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
      roles = %{"coordinator1" => :coordinator, "dbserver1" => :dbserver}
      merged = [{"coordinator1", e1}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, false, :basic, roles)
      assert result =~ "[CO1]"
      assert result =~ "[DB1]"
    end

    test "multi-server with color enabled includes ANSI escape sequences" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")
      e2 = entry(~U[2026-01-01 00:00:01Z], message: "line2")
      roles = %{"coordinator1" => :coordinator, "dbserver1" => :dbserver}
      merged = [{"coordinator1", e1}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, true, :basic, roles)
      assert result =~ "\e[38;5;"
    end

    test "WARNING level gets bright emphasis when color enabled" do
      e1 = entry(~U[2026-01-01 00:00:00Z], level: :warning, message: "msg")
      e2 = entry(~U[2026-01-01 00:00:01Z], message: "other")
      roles = %{"coordinator1" => :coordinator, "dbserver1" => :dbserver}
      merged = [{"coordinator1", e1}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, true, :basic, roles)
      assert result =~ IO.ANSI.bright()
    end

    test "ERROR level gets inverse emphasis when color enabled" do
      e1 = entry(~U[2026-01-01 00:00:00Z], level: :error, message: "msg")
      e2 = entry(~U[2026-01-01 00:00:01Z], message: "other")
      roles = %{"coordinator1" => :coordinator, "dbserver1" => :dbserver}
      merged = [{"coordinator1", e1}, {"dbserver1", e2}]
      result = Logs.format_merged(merged, true, :basic, roles)
      assert result =~ IO.ANSI.inverse()
    end
  end

  # --- server_tag/2 with hyphenated IDs ---

  describe "server_tag/2 with hyphenated IDs" do
    test "coordinator with cluster prefix" do
      assert Logs.server_tag("cluster-00-coordinator-0", :coordinator) == "CO0"
    end

    test "dbserver with cluster prefix" do
      assert Logs.server_tag("cluster-00-dbserver-2", :dbserver) == "DB2"
    end

    test "agent with cluster prefix" do
      assert Logs.server_tag("cluster-00-agent-1", :agent) == "AG1"
    end
  end

  # --- filter_servers/2 ---

  describe "filter_servers/2" do
    setup do
      servers = %{
        "coordinator1" => %{role: :coordinator},
        "dbserver1" => %{role: :dbserver},
        "agent1" => %{role: :agent}
      }

      %{servers: servers}
    end

    test "with :all filter returns all servers", %{servers: servers} do
      result = Logs.filter_servers(servers, :all)
      assert length(result) == 3
    end

    test "with role filter returns only matching roles", %{servers: servers} do
      result = Logs.filter_servers(servers, [{:role, :coordinator}])
      assert [{_id, _meta}] = result
      assert {"coordinator1", _} = hd(result)
    end

    test "with prefix filter returns matching prefix", %{servers: servers} do
      result = Logs.filter_servers(servers, [{:prefix, "db"}])
      assert [{"dbserver1", _}] = result
    end

    test "default filter excludes agents", %{servers: servers} do
      filter = Logs.parse_server_filter(nil)
      result = Logs.filter_servers(servers, filter)
      ids = Enum.map(result, &elem(&1, 0))
      assert "coordinator1" in ids
      assert "dbserver1" in ids
      refute "agent1" in ids
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
      result = Logs.matching_servers(servers, [{:role, :coordinator}])
      assert result == ["coordinator1"]
    end

    test "results are sorted", %{servers: servers} do
      result = Logs.matching_servers(servers, :all)
      assert result == Enum.sort(result)
    end
  end

  # --- extract/2 ---

  describe "extract/2" do
    setup do
      ts = to_us(~U[2026-03-09 10:00:00Z])

      servers = %{
        "coordinator1" => %{
          role: :coordinator,
          arango_id: nil,
          logs: [
            {to_us(~U[2026-03-09 09:59:50Z]), to_us(~U[2026-03-09 10:00:10Z]),
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
            {to_us(~U[2026-03-09 09:59:50Z]), to_us(~U[2026-03-09 10:00:10Z]),
             [entry(~U[2026-03-09 10:00:00Z], message: "agent-msg")]}
          ]
        }
      }

      # Crash default: 20s before, 0s after
      issue = %{type: :crash, time_bounds: {ts, ts}}
      window = Logs.display_window(issue, nil)

      %{servers: servers, window: window, ts: ts}
    end

    test "pre-filtered servers exclude agents", %{servers: servers, window: window} do
      filter = Logs.parse_server_filter(nil)
      filtered = Map.new(Logs.filter_servers(servers, filter))
      result = Logs.extract(filtered, window)
      server_ids = Enum.map(result, &elem(&1, 0))
      assert "coordinator1" in server_ids
      refute "agent1" in server_ids
    end

    test "unfiltered includes all servers", %{servers: servers, window: window} do
      result = Logs.extract(servers, window)
      server_ids = Enum.map(result, &elem(&1, 0))
      assert "agent1" in server_ids
    end

    test "filters entries by time window", %{servers: servers, window: window} do
      filtered = Map.new(Logs.filter_servers(servers, [{:role, :coordinator}]))
      [{_server, entries}] = Logs.extract(filtered, window)

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

      window = {to_us(~U[2026-03-09 10:00:00Z]), to_us(~U[2026-03-09 10:00:10Z])}
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

      window = {to_us(~U[2026-03-09 10:00:00Z]), to_us(~U[2026-03-09 10:00:10Z])}
      assert Logs.extract_events(events, window) == []
    end

    test "includes events at window boundaries" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      events = [%{event: :test_started, timestamp: ts, module: Mod, name: "t"}]
      window = {ts, ts}
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

  describe "format_merged with events" do
    @roles %{"coordinator1" => :coordinator, "dbserver1" => :dbserver}

    test "single server with events uses tagged format" do
      e1 = entry(~U[2026-01-01 00:00:00Z], message: "line1")

      event = %{
        event: :test_started,
        timestamp: to_us(~U[2026-01-01 00:00:01Z]),
        module: Mod,
        name: "t"
      }

      roles = %{"coordinator1" => :coordinator}
      merged = [{"coordinator1", e1}, {:event, event}]
      result = Logs.format_merged(merged, false, :basic, roles)

      assert result =~ "line1"
      assert result =~ ">>> test_started"
      assert result =~ "[CO1]"
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
      result = Logs.format_merged(merged, true, :basic, @roles)

      lines = String.split(result, "\n")
      event_line = Enum.find(lines, &String.contains?(&1, ">>>"))

      assert event_line != nil
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
      result = Logs.format_merged(merged, false, :basic, @roles)

      lines = String.split(result, "\n")
      tag_line = Enum.find(lines, &String.contains?(&1, "[CO1]"))
      event_line = Enum.find(lines, &String.contains?(&1, ">>>"))

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

      roles = %{"coordinator1" => :coordinator}
      merged = [{"coordinator1", e1}, {:event, event}]
      result = Logs.format_merged(merged, false, :full, roles)
      assert result =~ ">>> server_started db1 (pid=123)"
      assert result =~ "server_id: \"db1\""
    end
  end

  # --- parse_level_filter/1 ---

  describe "parse_level_filter/1" do
    test "nil returns nil" do
      assert Logs.parse_level_filter(nil) == nil
    end

    test "single level" do
      assert Logs.parse_level_filter("info") == %{global: :info, topics: %{}}
    end

    test "topic-specific" do
      assert Logs.parse_level_filter("crash=debug") == %{global: nil, topics: %{crash: :debug}}
    end

    test "combined global and topic" do
      assert Logs.parse_level_filter("info,crash=debug") == %{
               global: :info,
               topics: %{crash: :debug}
             }
    end

    test "multiple topics" do
      assert Logs.parse_level_filter("crash=debug,general=warning") == %{
               global: nil,
               topics: %{crash: :debug, general: :warning}
             }
    end
  end

  # --- level_passes?/2 ---

  describe "level_passes?/2" do
    test "nil filter passes everything" do
      assert Logs.level_passes?(%{level: :trace}, nil)
      assert Logs.level_passes?(%{level: :fatal}, nil)
    end

    test "global filter passes entries at or above level" do
      filter = Logs.parse_level_filter("warning")
      assert Logs.level_passes?(%{level: :warning}, filter)
      assert Logs.level_passes?(%{level: :error}, filter)
      assert Logs.level_passes?(%{level: :fatal}, filter)
    end

    test "global filter rejects entries below level" do
      filter = Logs.parse_level_filter("warning")
      refute Logs.level_passes?(%{level: :info}, filter)
      refute Logs.level_passes?(%{level: :debug}, filter)
      refute Logs.level_passes?(%{level: :trace}, filter)
    end

    test "topic override overrides global for that topic" do
      filter = Logs.parse_level_filter("warning,crash=debug")
      # crash topic at debug level passes (overrides global warning)
      assert Logs.level_passes?(%{level: :debug, topic: :crash}, filter)
      # non-crash topic at debug level fails (uses global warning)
      refute Logs.level_passes?(%{level: :debug, topic: :general}, filter)
    end

    test "entries without a level pass" do
      filter = Logs.parse_level_filter("error")
      assert Logs.level_passes?(%{message: "no level"}, filter)
    end

    test "entries without a topic use global level" do
      filter = Logs.parse_level_filter("warning")
      refute Logs.level_passes?(%{level: :info}, filter)
      assert Logs.level_passes?(%{level: :warning}, filter)
    end
  end

  # --- extract/3 with level filter ---

  describe "extract/3 with level filter" do
    setup do
      servers = %{
        "coordinator1" => %{
          role: :coordinator,
          logs: [
            {to_us(~U[2026-03-09 09:59:50Z]), to_us(~U[2026-03-09 10:00:10Z]),
             [
               entry(~U[2026-03-09 09:59:55Z], level: :debug, message: "debug-msg"),
               entry(~U[2026-03-09 10:00:00Z], level: :info, message: "info-msg"),
               entry(~U[2026-03-09 10:00:02Z], level: :warning, message: "warn-msg"),
               entry(~U[2026-03-09 10:00:03Z], level: :error, message: "error-msg")
             ]}
          ]
        }
      }

      window = {to_us(~U[2026-03-09 09:59:50Z]), to_us(~U[2026-03-09 10:00:10Z])}
      %{servers: servers, window: window}
    end

    test "filters entries by level within the time window", %{servers: servers, window: window} do
      filter = Logs.parse_level_filter("warning")
      [{_server, entries}] = Logs.extract(servers, window, level_filter: filter)

      messages = Enum.map(entries, & &1.message)
      assert "warn-msg" in messages
      assert "error-msg" in messages
      refute "info-msg" in messages
      refute "debug-msg" in messages
    end

    test "topic-specific override works", %{window: window} do
      servers = %{
        "coordinator1" => %{
          role: :coordinator,
          logs: [
            {to_us(~U[2026-03-09 09:59:50Z]), to_us(~U[2026-03-09 10:00:10Z]),
             [
               entry(~U[2026-03-09 10:00:00Z],
                 level: :debug,
                 topic: :crash,
                 message: "crash-dbg"
               ),
               entry(~U[2026-03-09 10:00:01Z],
                 level: :debug,
                 topic: :general,
                 message: "gen-dbg"
               ),
               entry(~U[2026-03-09 10:00:02Z],
                 level: :warning,
                 topic: :general,
                 message: "gen-warn"
               )
             ]}
          ]
        }
      }

      filter = Logs.parse_level_filter("warning,crash=debug")
      [{_server, entries}] = Logs.extract(servers, window, level_filter: filter)

      messages = Enum.map(entries, & &1.message)
      assert "crash-dbg" in messages
      refute "gen-dbg" in messages
      assert "gen-warn" in messages
    end
  end

  # --- extract with excluded_ids ---

  describe "extract with excluded_ids" do
    setup do
      servers = %{
        "coordinator1" => %{
          role: :coordinator,
          logs: [
            {to_us(~U[2026-03-09 09:59:50Z]), to_us(~U[2026-03-09 10:00:10Z]),
             [
               entry(~U[2026-03-09 10:00:00Z], id: "abc12", message: "keep-me"),
               entry(~U[2026-03-09 10:00:01Z], id: "e6460", message: "exclude-me"),
               entry(~U[2026-03-09 10:00:02Z], id: "def34", message: "also-keep")
             ]}
          ]
        }
      }

      window = {to_us(~U[2026-03-09 09:59:50Z]), to_us(~U[2026-03-09 10:00:10Z])}
      %{servers: servers, window: window}
    end

    test "excludes entries by ID", %{servers: servers, window: window} do
      excluded = Logs.parse_exclude("e6460")
      [{_server, entries}] = Logs.extract(servers, window, excluded_ids: excluded)

      messages = Enum.map(entries, & &1.message)
      assert "keep-me" in messages
      assert "also-keep" in messages
      refute "exclude-me" in messages
    end

    test "excludes multiple IDs", %{servers: servers, window: window} do
      excluded = Logs.parse_exclude("e6460,abc12")
      [{_server, entries}] = Logs.extract(servers, window, excluded_ids: excluded)

      messages = Enum.map(entries, & &1.message)
      refute "keep-me" in messages
      refute "exclude-me" in messages
      assert "also-keep" in messages
    end

    test "nil excluded_ids excludes nothing", %{servers: servers, window: window} do
      [{_server, entries}] = Logs.extract(servers, window, excluded_ids: nil)
      assert length(entries) == 3
    end

    test "entries without an ID are never excluded", %{window: window} do
      servers = %{
        "coordinator1" => %{
          role: :coordinator,
          logs: [
            {to_us(~U[2026-03-09 09:59:50Z]), to_us(~U[2026-03-09 10:00:10Z]),
             [entry(~U[2026-03-09 10:00:00Z], message: "no-id")]}
          ]
        }
      }

      excluded = Logs.parse_exclude("e6460")
      [{_server, entries}] = Logs.extract(servers, window, excluded_ids: excluded)
      assert length(entries) == 1
    end
  end

  # --- Helpers ---

  defp to_us(%DateTime{} = dt), do: DateTime.to_unix(dt, :microsecond)

  defp entry(time, opts) do
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
