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

defmodule ToastTest.LogAnalysisAndFormattingLogsTest do
  use ExUnit.Case, async: true

  alias ToastTest.Analyze.IssueStreams
  alias ToastTest.Analyze.Logs
  alias ToastTest.Formatting.Logs, as: LogFormatting

  import Toast.Utils, only: [maybe_put: 3]
  import ToastTest.TimeTestHelpers, only: [to_us: 1]

  @usec_per_sec 1_000_000
  @usec_per_ms 1_000

  # --- parse_server_filter/1 ---

  describe "parse_server_filter/1" do
    test "nil returns default (exclude agents)" do
      result = IssueStreams.parse_server_filter(nil)
      assert {:role, :coordinator} in result
      assert {:role, :dbserver} in result
      assert {:role, :single} in result

      refute Enum.any?(result, fn
               {:role, r} -> r == :agent
               _ -> false
             end)
    end

    test "\"all\" returns :all" do
      assert IssueStreams.parse_server_filter("all") == :all
    end

    test "known roles become :role filters" do
      assert IssueStreams.parse_server_filter("coordinator,dbserver") == [
               {:role, :coordinator},
               {:role, :dbserver}
             ]
    end

    test "unknown strings become :prefix filters" do
      assert IssueStreams.parse_server_filter("coordinator1,agent1") == [
               {:prefix, "coordinator1"},
               {:prefix, "agent1"}
             ]
    end

    test "mixed roles and prefixes" do
      assert IssueStreams.parse_server_filter("coordinator,agent1") == [
               {:role, :coordinator},
               {:prefix, "agent1"}
             ]
    end
  end

  # --- parse_window_spec/1 ---

  describe "parse_window_spec/1" do
    test "nil returns nil" do
      assert IssueStreams.parse_window_spec(nil) == nil
    end

    test "single number" do
      assert IssueStreams.parse_window_spec("30") == {30, 0}
    end

    test "before,after" do
      assert IssueStreams.parse_window_spec("30,5") == {30, 5}
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
      assert IssueStreams.server_matches?("coordinator1", :all, :coordinator)
      assert IssueStreams.server_matches?("agent1", :all, :agent)
    end

    test "role matching" do
      filter = [{:role, :coordinator}]
      assert IssueStreams.server_matches?("coordinator1", filter, :coordinator)
      assert IssueStreams.server_matches?("coordinator2", filter, :coordinator)
      refute IssueStreams.server_matches?("dbserver1", filter, :dbserver)
    end

    test "prefix matching" do
      filter = [{:prefix, "coordinator1"}]
      assert IssueStreams.server_matches?("coordinator1", filter, :coordinator)
      refute IssueStreams.server_matches?("coordinator2", filter, :coordinator)
    end

    test "multiple filters compose as union" do
      filter = [{:role, :coordinator}, {:prefix, "agent1"}]
      assert IssueStreams.server_matches?("coordinator1", filter, :coordinator)
      assert IssueStreams.server_matches?("agent1", filter, :agent)
      refute IssueStreams.server_matches?("dbserver1", filter, :dbserver)
      refute IssueStreams.server_matches?("agent2", filter, :agent)
    end

    test "role from metadata takes precedence over server ID" do
      filter = [{:role, :single}]
      assert IssueStreams.server_matches?("single-00", filter, :single)
    end
  end

  # --- display_window/2 ---

  describe "display_window/2" do
    test "nil time_bounds returns nil" do
      issue = %{type: :crash, time_bounds: nil}
      assert IssueStreams.display_window(issue, nil) == nil
    end

    test "crash default window" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      issue = %{type: :crash, time_bounds: {ts, ts}}
      {start_us, end_us} = IssueStreams.display_window(issue, nil)
      assert ts - start_us == 5 * @usec_per_sec
      assert end_us - ts == 0
    end

    test "test_failure default window" do
      s = to_us(~U[2026-03-09 10:00:00Z])
      f = to_us(~U[2026-03-09 10:00:05Z])
      issue = %{type: :test_failure, time_bounds: {s, f}}
      {start_us, end_us} = IssueStreams.display_window(issue, nil)
      assert s - start_us == 100 * @usec_per_ms
      assert end_us - f == 100 * @usec_per_ms
    end

    test "timeout default window" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      issue = %{type: :timeout, time_bounds: {ts, ts}}
      {start_us, end_us} = IssueStreams.display_window(issue, nil)
      assert ts - start_us == 5 * @usec_per_sec
      assert end_us - ts == 0
    end

    test "sanitizer_report default window" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      issue = %{type: :sanitizer_report, time_bounds: {ts, ts}}
      {start_us, end_us} = IssueStreams.display_window(issue, nil)
      assert ts - start_us == 100 * @usec_per_ms
      assert end_us - ts == 100 * @usec_per_ms
    end

    test "custom window spec overrides defaults" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      issue = %{type: :crash, time_bounds: {ts, ts}}
      {start_us, end_us} = IssueStreams.display_window(issue, {-30_000, 10_000})
      assert ts - start_us == 30 * @usec_per_sec
      assert end_us - ts == 10 * @usec_per_sec
    end
  end

  # --- server_tag/2 ---

  describe "server_tag/2" do
    test "coordinator" do
      assert LogFormatting.server_tag("coordinator1", :coordinator) == "CO1"
      assert LogFormatting.server_tag("coordinator2", :coordinator) == "CO2"
    end

    test "dbserver" do
      assert LogFormatting.server_tag("dbserver1", :dbserver) == "DB1"
      assert LogFormatting.server_tag("dbserver2", :dbserver) == "DB2"
    end

    test "agent" do
      assert LogFormatting.server_tag("agent1", :agent) == "AG1"
    end

    test "single" do
      assert LogFormatting.server_tag("single", :single) == "SNG"
    end

    test "unconventional name with role from metadata" do
      assert LogFormatting.server_tag("single-00", :single) == "SNG00"
    end
  end

  # --- server_color/2 ---

  describe "server_color/2" do
    test "coordinator returns a color from the coordinator palette" do
      assert LogFormatting.server_color(:coordinator, 0) in [
               67,
               103,
               110,
               66,
               109,
               60,
               68,
               102,
               146
             ]
    end

    test "dbserver returns a color from the dbserver palette" do
      assert LogFormatting.server_color(:dbserver, 0) in [
               137,
               174,
               95,
               180,
               130,
               215,
               101,
               172,
               144
             ]
    end

    test "agent returns a color from the agent palette" do
      assert LogFormatting.server_color(:agent, 0) in [101, 138, 66, 144, 96]
    end

    test "different indices return different colors" do
      assert LogFormatting.server_color(:coordinator, 0) !=
               LogFormatting.server_color(:coordinator, 1)
    end

    test "single role uses dbserver palette" do
      assert LogFormatting.server_color(:single, 0) in [
               137,
               174,
               95,
               180,
               130,
               215,
               101,
               172,
               144
             ]
    end

    test "unknown role falls back to coordinator palette" do
      assert LogFormatting.server_color(:unknown, 0) in [67, 103, 110, 66, 109, 60, 68, 102, 146]
    end
  end

  # --- server_tag/2 with hyphenated IDs ---

  describe "server_tag/2 with hyphenated IDs" do
    test "coordinator with deployment prefix" do
      assert LogFormatting.server_tag("cluster-00-coordinator-0", :coordinator) == "CO0"
    end

    test "dbserver with deployment prefix" do
      assert LogFormatting.server_tag("cluster-00-dbserver-2", :dbserver) == "DB2"
    end

    test "agent with deployment prefix" do
      assert LogFormatting.server_tag("cluster-00-agent-1", :agent) == "AG1"
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
      result = IssueStreams.filter_servers(servers, :all)
      assert length(result) == 3
    end

    test "with role filter returns only matching roles", %{servers: servers} do
      result = IssueStreams.filter_servers(servers, [{:role, :coordinator}])
      assert [{_id, _meta}] = result
      assert {"coordinator1", _} = hd(result)
    end

    test "with prefix filter returns matching prefix", %{servers: servers} do
      result = IssueStreams.filter_servers(servers, [{:prefix, "db"}])
      assert [{"dbserver1", _}] = result
    end

    test "default filter excludes agents", %{servers: servers} do
      filter = IssueStreams.parse_server_filter(nil)
      result = IssueStreams.filter_servers(servers, filter)
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
      result = IssueStreams.matching_servers(servers, :all)
      assert length(result) == 3
    end

    test "with role filter returns only matching roles", %{servers: servers} do
      result = IssueStreams.matching_servers(servers, [{:role, :coordinator}])
      assert result == ["coordinator1"]
    end

    test "results are sorted", %{servers: servers} do
      result = IssueStreams.matching_servers(servers, :all)
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

      # Crash display default: 5s before, 0s after
      issue = %{type: :crash, time_bounds: {ts, ts}}
      window = IssueStreams.display_window(issue, nil)

      %{servers: servers, window: window, ts: ts}
    end

    test "pre-filtered servers exclude agents", %{servers: servers, window: window} do
      filter = IssueStreams.parse_server_filter(nil)
      filtered = Map.new(IssueStreams.filter_servers(servers, filter))
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
      filtered = Map.new(IssueStreams.filter_servers(servers, [{:role, :coordinator}]))
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
      result = IssueStreams.extract_events(events, window)

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
      assert IssueStreams.extract_events(events, window) == []
    end

    test "includes events at window boundaries" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      events = [%{event: :test_started, timestamp: ts, module: Mod, name: "t"}]
      window = {ts, ts}
      assert length(IssueStreams.extract_events(events, window)) == 1
    end
  end

  # --- format_event/1 ---

  describe "format_event/1" do
    test "server_started" do
      event = %{event: :server_started, server_id: "dbserver1", pid: 12_345, timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> server_started dbserver1 (pid=12345)"
    end

    test "server_stopped" do
      event = %{event: :server_stopped, server_id: "dbserver1", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> server_stopped dbserver1"
    end

    test "server_crashed" do
      event = %{
        event: :server_crashed,
        server_id: "dbserver1",
        pid: 123,
        signal: 11,
        timestamp: 0
      }

      assert LogFormatting.format_event(event) ==
               ">>> server_crashed dbserver1 (pid=123, signal=11)"
    end

    test "server_killed" do
      event = %{event: :server_killed, server_id: "s1", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> server_killed s1"
    end

    test "server_paused" do
      event = %{event: :server_paused, server_id: "s1", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> server_paused s1"
    end

    test "server_resumed" do
      event = %{event: :server_resumed, server_id: "s1", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> server_resumed s1"
    end

    test "test_started" do
      event = %{event: :test_started, module: MyModule, name: "my test", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> test_started MyModule > my test"
    end

    test "test_finished" do
      event = %{
        event: :test_finished,
        module: MyModule,
        name: "my test",
        outcome: :passed,
        timestamp: 0
      }

      assert LogFormatting.format_event(event) == ">>> test_finished MyModule > my test (passed)"
    end

    test "module_started" do
      event = %{event: :module_started, module: MyModule, timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> module_started MyModule"
    end

    test "module_finished" do
      event = %{event: :module_finished, module: MyModule, timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> module_finished MyModule"
    end

    test "deployment_starting" do
      event = %{event: :deployment_starting, deployment_id: "d1", mode: :cluster, timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> deployment_starting d1 (cluster)"
    end

    test "deployment_started" do
      event = %{event: :deployment_started, deployment_id: "d1", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> deployment_started d1"
    end

    test "deployment_stopped" do
      event = %{event: :deployment_stopped, deployment_id: "d1", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> deployment_stopped d1"
    end

    test "timeout_kill" do
      event = %{event: :timeout_kill, reason: "test exceeded 60s", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> timeout_kill test exceeded 60s"
    end

    test "server_identified" do
      event = %{event: :server_identified, server_id: "s1", arango_id: "CRDN-abc", timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> server_identified s1 => CRDN-abc"
    end

    test "unknown event" do
      event = %{event: :something_new, timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> something_new"
    end

    test "custom event with payload" do
      event = %{
        event: :custom,
        kind: :cache_invalidated,
        payload: %{key: "foo", reason: :ttl},
        timestamp: 0
      }

      formatted = LogFormatting.format_event(event)
      assert String.starts_with?(formatted, ">>> custom:cache_invalidated ")
      assert formatted =~ "key: \"foo\""
      assert formatted =~ "reason: :ttl"
    end

    test "custom event with empty payload" do
      event = %{event: :custom, kind: :checkpoint, payload: %{}, timestamp: 0}
      assert LogFormatting.format_event(event) == ">>> custom:checkpoint"
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

  # --- merge/1 ---

  describe "merge/1" do
    test "empty input returns empty list" do
      assert IssueStreams.merge([]) == []
    end

    test "single stream returns entries tagged with stream tag" do
      entries = [
        entry(~U[2026-03-09 10:00:00Z], message: "first"),
        entry(~U[2026-03-09 10:00:01Z], message: "second")
      ]

      result = IssueStreams.merge([{"coordinator1", entries}])

      assert [{"coordinator1", e1}, {"coordinator1", e2}] = result
      assert e1.message == "first"
      assert e2.message == "second"
    end

    test "two streams interleaved chronologically" do
      co_entries = [
        entry(~U[2026-03-09 10:00:00Z], message: "co-1"),
        entry(~U[2026-03-09 10:00:02Z], message: "co-2")
      ]

      db_entries = [
        entry(~U[2026-03-09 10:00:01Z], message: "db-1"),
        entry(~U[2026-03-09 10:00:03Z], message: "db-2")
      ]

      result = IssueStreams.merge([{"coordinator1", co_entries}, {"dbserver1", db_entries}])

      assert Enum.map(result, &elem(&1, 0)) == [
               "coordinator1",
               "dbserver1",
               "coordinator1",
               "dbserver1"
             ]

      assert Enum.map(result, fn {_, e} -> e.message end) == [
               "co-1",
               "db-1",
               "co-2",
               "db-2"
             ]
    end

    test "events as explicit stream using :timestamp key" do
      events = [
        %{
          event: :test_started,
          timestamp: to_us(~U[2026-03-09 10:00:00Z]),
          module: Mod,
          name: "t"
        },
        %{
          event: :test_finished,
          timestamp: to_us(~U[2026-03-09 10:00:02Z]),
          module: Mod,
          name: "t",
          outcome: :passed
        }
      ]

      result = IssueStreams.merge([{:event, events}])

      assert [{:event, e1}, {:event, e2}] = result
      assert e1.event == :test_started
      assert e2.event == :test_finished
    end

    test "traffic as explicit stream using :timestamp key" do
      traffic = [
        %{timestamp: to_us(~U[2026-03-09 10:00:00Z]), method: "GET", path: "/api/v1"},
        %{timestamp: to_us(~U[2026-03-09 10:00:01Z]), method: "POST", path: "/api/v1"}
      ]

      result = IssueStreams.merge([{:traffic, traffic}])

      assert [{:traffic, t1}, {:traffic, t2}] = result
      assert t1.method == "GET"
      assert t2.method == "POST"
    end

    test "mixed log, event, and traffic streams merged by time" do
      logs = [
        entry(~U[2026-03-09 10:00:00Z], message: "log-1"),
        entry(~U[2026-03-09 10:00:03Z], message: "log-2")
      ]

      events = [
        %{
          event: :test_started,
          timestamp: to_us(~U[2026-03-09 10:00:01Z]),
          module: Mod,
          name: "t"
        }
      ]

      traffic = [
        %{timestamp: to_us(~U[2026-03-09 10:00:02Z]), method: "GET", path: "/api"}
      ]

      result =
        IssueStreams.merge([
          {"coordinator1", logs},
          {:event, events},
          {:traffic, traffic}
        ])

      assert Enum.map(result, &elem(&1, 0)) == [
               "coordinator1",
               :event,
               :traffic,
               "coordinator1"
             ]
    end

    test "empty streams are filtered out" do
      entry1 = entry(~U[2026-03-09 10:00:00Z], message: "only-entry")

      result = IssueStreams.merge([{"s1", []}, {"s2", [entry1]}])

      assert [{"s2", e}] = result
      assert e.message == "only-entry"
    end

    test "single entry per stream merges chronologically" do
      e1 = entry(~U[2026-03-09 10:00:02Z], message: "second")
      e2 = entry(~U[2026-03-09 10:00:00Z], message: "first")
      e3 = entry(~U[2026-03-09 10:00:01Z], message: "middle")

      result = IssueStreams.merge([{"a", [e1]}, {"b", [e2]}, {"c", [e3]}])

      assert Enum.map(result, fn {tag, _} -> tag end) == ["b", "c", "a"]

      assert Enum.map(result, fn {_, e} -> e.message end) == [
               "first",
               "middle",
               "second"
             ]
    end

    test "preserves tag unchanged for atom, string, and tuple tags" do
      atom_entry = %{timestamp: to_us(~U[2026-03-09 10:00:00Z]), data: "atom-tagged"}
      string_entry = entry(~U[2026-03-09 10:00:01Z], message: "string-tagged")
      tuple_entry = %{timestamp: to_us(~U[2026-03-09 10:00:02Z]), data: "tuple-tagged"}

      result =
        IssueStreams.merge([
          {:my_atom, [atom_entry]},
          {"my_string", [string_entry]},
          {{:deployment, "d1"}, [tuple_entry]}
        ])

      tags = Enum.map(result, &elem(&1, 0))
      assert tags == [:my_atom, "my_string", {:deployment, "d1"}]
    end
  end

  # --- Helpers ---

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
end
