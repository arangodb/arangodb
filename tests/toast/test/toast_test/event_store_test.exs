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

defmodule ToastTest.EventStoreTest do
  use ExUnit.Case, async: false

  alias ToastTest.EventStore

  import ToastTest.TimeTestHelpers, only: [to_us: 1]

  setup do
    EventStore.clear()
    on_exit(fn -> EventStore.clear() end)
    :ok
  end

  describe "start_link/1" do
    test "starts the GenServer" do
      pid = Process.whereis(EventStore)
      assert pid != nil
      assert Process.alive?(pid)
    end

    test "ETS table exists" do
      assert :ets.info(ToastTest.EventStore) != :undefined
    end
  end

  describe "events/0" do
    test "returns empty list when no events recorded" do
      assert EventStore.events() == []
    end

    test "records events in chronological order" do
      t1 = to_us(~U[2026-03-09 10:00:00Z])
      t2 = to_us(~U[2026-03-09 10:00:01Z])
      t3 = to_us(~U[2026-03-09 10:00:02Z])

      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 100, timestamp: t1})
      EventStore.notify(%{event: :server_stopped, server_id: "s1", pid: 100, timestamp: t2})
      EventStore.notify(%{event: :server_started, server_id: "s2", pid: 200, timestamp: t3})

      events = EventStore.events()
      assert length(events) == 3
      assert Enum.map(events, & &1.event) == [:server_started, :server_stopped, :server_started]
      assert Enum.map(events, & &1.server_id) == ["s1", "s1", "s2"]
    end

    test "preserves timestamps" do
      now = to_us(~U[2026-03-09 10:00:00Z])
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 100, timestamp: now})

      [recorded] = EventStore.events()
      assert recorded.timestamp == now
    end
  end

  describe "pids_by_server/0" do
    test "returns empty map when no events recorded" do
      assert EventStore.pids_by_server() == %{}
    end

    test "collects OS PIDs from server_started events" do
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})
      EventStore.notify(%{event: :server_started, server_id: "s2", pid: 1002, timestamp: ts()})

      assert EventStore.pids_by_server() == %{"s1" => [1001], "s2" => [1002]}
    end

    test "collects multiple PIDs for the same server (relaunch)" do
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 1002, timestamp: ts()})

      result = EventStore.pids_by_server()
      assert result["s1"] == [1001, 1002]
    end

    test "deduplicates repeated PIDs" do
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})

      assert EventStore.pids_by_server() == %{"s1" => [1001]}
    end

    test "ignores non-server_started events" do
      EventStore.notify(%{event: :server_stopped, server_id: "s1", pid: 1001, timestamp: ts()})

      assert EventStore.pids_by_server() == %{}
    end
  end

  describe "unexpected_crashes/0" do
    test "returns empty list when no events recorded" do
      assert EventStore.unexpected_crashes() == []
    end

    test "returns only unexpected crash events" do
      EventStore.notify(%{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s1",
        pid: 100,
        crash_info: %{signal: 11},
        expected: false,
        timestamp: ts()
      })

      EventStore.notify(%{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s2",
        pid: 200,
        crash_info: %{signal: 11},
        expected: true,
        timestamp: ts()
      })

      crashes = EventStore.unexpected_crashes()
      assert length(crashes) == 1
      assert hd(crashes).server_id == "s1"
    end

    test "returns crashes in chronological order" do
      for i <- 1..3 do
        EventStore.notify(%{
          event: :server_crashed,
          deployment_id: "d1",
          server_id: "s#{i}",
          pid: 100 + i,
          crash_info: %{signal: 11},
          expected: false,
          timestamp: ts()
        })
      end

      crashes = EventStore.unexpected_crashes()
      assert Enum.map(crashes, & &1.server_id) == ["s1", "s2", "s3"]
    end
  end

  describe "timeout_kills/0" do
    test "returns empty list when no timeout events" do
      assert EventStore.timeout_kills() == []
    end

    test "records and retrieves timeout kill events" do
      EventStore.notify(%{
        event: :timeout_kill,
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "s1", os_pid: 1001}],
        timestamp: ts()
      })

      kills = EventStore.timeout_kills()
      assert length(kills) == 1
      assert hd(kills).source == :suite_timeout
    end

    test "returns multiple timeout kills in chronological order" do
      base = to_us(~U[2026-03-09 10:00:00Z])

      for {source, i} <- Enum.with_index([:suite_timeout, :global_timeout]) do
        EventStore.notify(%{
          event: :timeout_kill,
          source: source,
          reason: "Timeout #{i}",
          servers: [],
          timestamp: base + i * 60_000_000
        })
      end

      kills = EventStore.timeout_kills()
      assert Enum.map(kills, & &1.source) == [:suite_timeout, :global_timeout]
    end

    test "ignores non-timeout events" do
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})

      assert EventStore.timeout_kills() == []
    end
  end

  describe "clear/0" do
    test "removes all recorded events" do
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 100, timestamp: ts()})
      EventStore.notify(%{event: :server_stopped, server_id: "s1", pid: 100, timestamp: ts()})

      assert length(EventStore.events()) == 2

      EventStore.clear()

      assert EventStore.events() == []
    end

    test "events can be recorded after clear" do
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 100, timestamp: ts()})
      EventStore.clear()
      EventStore.notify(%{event: :server_started, server_id: "s2", pid: 200, timestamp: ts()})

      events = EventStore.events()
      assert length(events) == 1
      assert hd(events).server_id == "s2"
    end
  end

  describe "deployments/0" do
    test "returns empty map when no deployments" do
      assert EventStore.deployments() == %{}
    end

    test "reconstructs deployment from starting event" do
      ts = to_us(~U[2026-03-09 10:00:00Z])
      start_deployment("d1", mode: :cluster, timestamp: ts)

      deployments = EventStore.deployments()
      assert %{"d1" => d} = deployments
      assert d.mode == :cluster
      assert d.started_at == ts
      assert d.stopped_at == nil
    end

    test "records stop time" do
      t1 = to_us(~U[2026-03-09 10:00:00Z])
      t2 = to_us(~U[2026-03-09 10:05:00Z])

      start_deployment("d1", timestamp: t1)

      EventStore.notify(%{
        event: :deployment_stopped,
        deployment_id: "d1",
        timestamp: t2
      })

      %{"d1" => d} = EventStore.deployments()
      assert d.started_at == t1
      assert d.stopped_at == t2
    end

    test "tracks multiple deployments" do
      start_deployment("d1", mode: :cluster)
      start_deployment("d2", mode: :single_server)

      deployments = EventStore.deployments()
      assert map_size(deployments) == 2
      assert deployments["d1"].mode == :cluster
      assert deployments["d2"].mode == :single_server
    end
  end

  describe "servers/0" do
    test "returns empty map when no deployments" do
      assert EventStore.servers() == %{}
    end

    test "reconstructs servers from deployment_started event" do
      start_deployment("d1",
        servers: %{
          "s1" => %{
            role: :coordinator,
            endpoint: "http://localhost:8529",
            log_file: "/tmp/s1.log"
          },
          "s2" => %{role: :dbserver, endpoint: "http://localhost:8530", log_file: "/tmp/s2.log"}
        }
      )

      %{"d1" => servers} = EventStore.servers()
      assert map_size(servers) == 2
      assert servers["s1"].role == :coordinator
      assert servers["s1"].endpoint == "http://localhost:8529"
      assert servers["s1"].log_file == "/tmp/s1.log"
      assert servers["s1"].deployment_id == "d1"
      assert servers["s1"].incarnations == []
    end

    test "tracks incarnations from server_started/stopped events" do
      t0 = to_us(~U[2026-03-09 09:59:00Z])
      t1 = to_us(~U[2026-03-09 10:00:00Z])
      t2 = to_us(~U[2026-03-09 10:01:00Z])
      t3 = to_us(~U[2026-03-09 10:02:00Z])
      t4 = to_us(~U[2026-03-09 10:03:00Z])

      start_deployment("d1", servers: %{"s1" => %{role: :coordinator}}, timestamp: t0)

      # First incarnation
      EventStore.notify(%{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t1
      })

      EventStore.notify(%{
        event: :server_stopped,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t2
      })

      # Second incarnation (restart)
      EventStore.notify(%{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 2002,
        timestamp: t3
      })

      EventStore.notify(%{
        event: :server_stopped,
        deployment_id: "d1",
        server_id: "s1",
        pid: 2002,
        timestamp: t4
      })

      %{"d1" => %{"s1" => server}} = EventStore.servers()
      assert length(server.incarnations) == 2

      [first, second] = server.incarnations
      assert first.pid == 1001
      assert first.started_at == t1
      assert first.stopped_at == t2
      assert second.pid == 2002
      assert second.started_at == t3
      assert second.stopped_at == t4
    end

    test "crash closes the current incarnation" do
      t0 = to_us(~U[2026-03-09 09:59:00Z])
      t1 = to_us(~U[2026-03-09 10:00:00Z])
      t2 = to_us(~U[2026-03-09 10:01:00Z])

      start_deployment("d1", servers: %{"s1" => %{role: :dbserver}}, timestamp: t0)

      EventStore.notify(%{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t1
      })

      EventStore.notify(%{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        crash_info: %{signal: 11},
        expected: false,
        timestamp: t2
      })

      %{"d1" => %{"s1" => server}} = EventStore.servers()
      assert [inc] = server.incarnations
      assert inc.stopped_at == t2
    end

    test "server_identified sets arango_id" do
      start_deployment("d1", servers: %{"s1" => %{role: :coordinator}})

      EventStore.notify(%{
        event: :server_identified,
        deployment_id: "d1",
        server_id: "s1",
        arango_id: "CRDN-abc123",
        timestamp: ts()
      })

      %{"d1" => %{"s1" => server}} = EventStore.servers()
      assert server.arango_id == "CRDN-abc123"
    end

    test "multiple deployments tracked independently" do
      start_deployment("d1", servers: %{"s1" => %{role: :coordinator}})
      start_deployment("d2", mode: :single_server, servers: %{"s2" => %{role: :single}})

      servers = EventStore.servers()
      assert map_size(servers) == 2
      assert Map.has_key?(servers["d1"], "s1")
      assert Map.has_key?(servers["d2"], "s2")
    end
  end

  describe "snapshot/0" do
    test "returns all projections consistent with individual queries" do
      t1 = to_us(~U[2026-03-09 10:00:00Z])
      t2 = to_us(~U[2026-03-09 10:01:00Z])
      t3 = to_us(~U[2026-03-09 10:02:00Z])
      t4 = to_us(~U[2026-03-09 10:03:00Z])

      start_deployment("d1",
        mode: :cluster,
        servers: %{
          "s1" => %{role: :coordinator, endpoint: "http://localhost:8529"}
        }
      )

      EventStore.notify(%{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t1
      })

      EventStore.notify(%{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        crash_info: %{signal: 11},
        expected: false,
        timestamp: t2
      })

      EventStore.notify(%{
        event: :server_identified,
        deployment_id: "d1",
        server_id: "s1",
        arango_id: "CRDN-abc",
        timestamp: t3
      })

      EventStore.notify(%{
        event: :timeout_kill,
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "s1", os_pid: 1001}],
        timestamp: t4
      })

      snapshot = EventStore.snapshot()

      assert Map.keys(snapshot) |> Enum.sort() ==
               [
                 :deployments,
                 :events,
                 :pids_by_server,
                 :servers,
                 :timeout_kills,
                 :unexpected_crashes
               ]

      assert snapshot.events == EventStore.events()
      assert snapshot.pids_by_server == EventStore.pids_by_server()
      assert snapshot.unexpected_crashes == EventStore.unexpected_crashes()
      assert snapshot.timeout_kills == EventStore.timeout_kills()
      assert snapshot.deployments == EventStore.deployments()
      assert snapshot.servers == EventStore.servers()
    end
  end

  describe "edge cases" do
    test "deployment_started without prior deployment_starting" do
      ts = to_us(~U[2026-03-09 10:00:00Z])

      EventStore.notify(%{
        event: :deployment_started,
        deployment_id: "d1",
        servers: %{},
        timestamp: ts
      })

      %{"d1" => d} = EventStore.deployments()
      assert d.mode == nil
      assert d.stacktrace == nil
      assert d.started_at == ts
    end

    test "deployment_stopped for unknown deployment" do
      ts = to_us(~U[2026-03-09 10:00:00Z])

      EventStore.notify(%{
        event: :deployment_stopped,
        deployment_id: "d_unknown",
        timestamp: ts
      })

      %{"d_unknown" => d} = EventStore.deployments()
      assert d.stopped_at == ts
    end

    test "server_identified for unknown server" do
      EventStore.notify(%{
        event: :server_identified,
        deployment_id: "d1",
        server_id: "s_unknown",
        arango_id: "CRDN-xyz",
        timestamp: ts()
      })

      assert EventStore.servers() == %{}
    end

    test "server_started without deployment_started" do
      EventStore.notify(%{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: ts()
      })

      assert EventStore.pids_by_server() == %{"s1" => [1001]}
      assert EventStore.servers() == %{}
    end

    test "server_crashed with expected: true does not appear in unexpected_crashes" do
      t0 = to_us(~U[2026-03-09 09:59:00Z])
      t1 = to_us(~U[2026-03-09 10:00:00Z])
      t2 = to_us(~U[2026-03-09 10:01:00Z])

      start_deployment("d1", servers: %{"s1" => %{role: :dbserver}}, timestamp: t0)

      EventStore.notify(%{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t1
      })

      EventStore.notify(%{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        crash_info: %{signal: 15},
        expected: true,
        timestamp: t2
      })

      assert EventStore.unexpected_crashes() == []

      %{"d1" => %{"s1" => server}} = EventStore.servers()
      assert [inc] = server.incarnations
      assert inc.stopped_at == t2
    end
  end

  # --- Helpers ---

  defp ts, do: :os.system_time(:microsecond)

  defp start_deployment(did, opts) do
    mode = Keyword.get(opts, :mode, :cluster)
    servers = Keyword.get(opts, :servers, %{})
    timestamp = Keyword.get(opts, :timestamp, ts())

    EventStore.notify(%{
      event: :deployment_starting,
      deployment_id: did,
      mode: mode,
      stacktrace: Keyword.get(opts, :stacktrace),
      specs: [],
      timestamp: timestamp
    })

    EventStore.notify(%{
      event: :deployment_started,
      deployment_id: did,
      servers: servers,
      timestamp: timestamp
    })
  end
end
