defmodule ToastTest.ProcessHistoryTest do
  use ExUnit.Case, async: true

  alias ToastTest.ProcessHistory

  setup do
    name = :"process_history_#{System.unique_integer([:positive])}"
    {:ok, pid} = ProcessHistory.start_link(name: name)

    on_exit(fn ->
      try do
        GenServer.stop(pid)
      catch
        :exit, _ -> :ok
      end
    end)

    %{pid: pid, name: name}
  end

  describe "start_link/1" do
    test "starts the GenServer", %{pid: pid} do
      assert Process.alive?(pid)
    end

    test "registers with given name", %{name: name} do
      assert Process.whereis(name) != nil
    end
  end

  describe "notify/1" do
    test "silently drops events when not running" do
      assert :ok = ProcessHistory.notify(%{event: :test, timestamp: DateTime.utc_now()})
    end
  end

  describe "events/1" do
    test "returns empty list when no events recorded", %{name: name} do
      assert ProcessHistory.events(name) == []
    end

    test "records events in chronological order", %{name: name} do
      t1 = ~U[2026-03-09 10:00:00Z]
      t2 = ~U[2026-03-09 10:00:01Z]
      t3 = ~U[2026-03-09 10:00:02Z]

      cast(name, %{event: :server_started, server_id: "s1", pid: 100, timestamp: t1})
      cast(name, %{event: :server_stopped, server_id: "s1", pid: 100, timestamp: t2})
      cast(name, %{event: :server_started, server_id: "s2", pid: 200, timestamp: t3})

      events = ProcessHistory.events(name)
      assert length(events) == 3
      assert Enum.map(events, & &1.event) == [:server_started, :server_stopped, :server_started]
      assert Enum.map(events, & &1.server_id) == ["s1", "s1", "s2"]
    end

    test "preserves timestamps", %{name: name} do
      now = ~U[2026-03-09 10:00:00Z]
      cast(name, %{event: :server_started, server_id: "s1", pid: 100, timestamp: now})

      [recorded] = ProcessHistory.events(name)
      assert recorded.timestamp == now
    end
  end

  describe "pids_by_server/1" do
    test "returns empty map when no events recorded", %{name: name} do
      assert ProcessHistory.pids_by_server(name) == %{}
    end

    test "collects OS PIDs from server_started events", %{name: name} do
      cast(name, %{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})
      cast(name, %{event: :server_started, server_id: "s2", pid: 1002, timestamp: ts()})

      assert ProcessHistory.pids_by_server(name) == %{"s1" => [1001], "s2" => [1002]}
    end

    test "collects multiple PIDs for the same server (relaunch)", %{name: name} do
      cast(name, %{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})
      cast(name, %{event: :server_started, server_id: "s1", pid: 1002, timestamp: ts()})

      result = ProcessHistory.pids_by_server(name)
      assert result["s1"] == [1001, 1002]
    end

    test "deduplicates repeated PIDs", %{name: name} do
      cast(name, %{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})
      cast(name, %{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})

      assert ProcessHistory.pids_by_server(name) == %{"s1" => [1001]}
    end

    test "ignores non-server_started events", %{name: name} do
      cast(name, %{event: :server_stopped, server_id: "s1", pid: 1001, timestamp: ts()})

      assert ProcessHistory.pids_by_server(name) == %{}
    end
  end

  describe "unexpected_crashes/1" do
    test "returns empty list when no events recorded", %{name: name} do
      assert ProcessHistory.unexpected_crashes(name) == []
    end

    test "returns only unexpected crash events", %{name: name} do
      cast(name, %{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s1",
        pid: 100,
        crash_info: %{signal: 11},
        expected: false,
        timestamp: ts()
      })

      cast(name, %{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s2",
        pid: 200,
        crash_info: %{signal: 11},
        expected: true,
        timestamp: ts()
      })

      crashes = ProcessHistory.unexpected_crashes(name)
      assert length(crashes) == 1
      assert hd(crashes).server_id == "s1"
    end

    test "returns crashes in chronological order", %{name: name} do
      for i <- 1..3 do
        cast(name, %{
          event: :server_crashed,
          deployment_id: "d1",
          server_id: "s#{i}",
          pid: 100 + i,
          crash_info: %{signal: 11},
          expected: false,
          timestamp: ts()
        })
      end

      crashes = ProcessHistory.unexpected_crashes(name)
      assert Enum.map(crashes, & &1.server_id) == ["s1", "s2", "s3"]
    end
  end

  describe "timeout_kills/1" do
    test "returns empty list when no timeout events", %{name: name} do
      assert ProcessHistory.timeout_kills(name) == []
    end

    test "records and retrieves timeout kill events", %{name: name} do
      cast(name, %{
        event: :timeout_kill,
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "s1", os_pid: 1001}],
        timestamp: ts()
      })

      kills = ProcessHistory.timeout_kills(name)
      assert length(kills) == 1
      assert hd(kills).source == :suite_timeout
    end

    test "returns multiple timeout kills in chronological order", %{name: name} do
      for {source, i} <- Enum.with_index([:suite_timeout, :global_timeout]) do
        cast(name, %{
          event: :timeout_kill,
          source: source,
          reason: "Timeout #{i}",
          servers: [],
          timestamp: DateTime.add(~U[2026-03-09 10:00:00Z], i, :minute)
        })
      end

      kills = ProcessHistory.timeout_kills(name)
      assert Enum.map(kills, & &1.source) == [:suite_timeout, :global_timeout]
    end

    test "ignores non-timeout events", %{name: name} do
      cast(name, %{event: :server_started, server_id: "s1", pid: 1001, timestamp: ts()})

      assert ProcessHistory.timeout_kills(name) == []
    end
  end

  describe "clear/0" do
    test "removes all recorded events", %{name: name} do
      cast(name, %{event: :server_started, server_id: "s1", pid: 100, timestamp: ts()})
      cast(name, %{event: :server_stopped, server_id: "s1", pid: 100, timestamp: ts()})

      assert length(ProcessHistory.events(name)) == 2

      GenServer.cast(name, :clear)

      assert ProcessHistory.events(name) == []
    end

    test "events can be recorded after clear", %{name: name} do
      cast(name, %{event: :server_started, server_id: "s1", pid: 100, timestamp: ts()})
      GenServer.cast(name, :clear)
      cast(name, %{event: :server_started, server_id: "s2", pid: 200, timestamp: ts()})

      events = ProcessHistory.events(name)
      assert length(events) == 1
      assert hd(events).server_id == "s2"
    end
  end

  describe "deployments/1" do
    test "returns empty map when no deployments", %{name: name} do
      assert ProcessHistory.deployments(name) == %{}
    end

    test "reconstructs deployment from starting event", %{name: name} do
      ts = ~U[2026-03-09 10:00:00Z]
      start_deployment(name, "d1", mode: :cluster, timestamp: ts)

      deployments = ProcessHistory.deployments(name)
      assert %{"d1" => d} = deployments
      assert d.mode == :cluster
      assert d.started_at == ts
      assert d.stopped_at == nil
    end

    test "records stop time", %{name: name} do
      t1 = ~U[2026-03-09 10:00:00Z]
      t2 = ~U[2026-03-09 10:05:00Z]

      start_deployment(name, "d1", timestamp: t1)

      cast(name, %{
        event: :deployment_stopped,
        deployment_id: "d1",
        timestamp: t2
      })

      %{"d1" => d} = ProcessHistory.deployments(name)
      assert d.started_at == t1
      assert d.stopped_at == t2
    end

    test "tracks multiple deployments", %{name: name} do
      start_deployment(name, "d1", mode: :cluster)
      start_deployment(name, "d2", mode: :single_server)

      deployments = ProcessHistory.deployments(name)
      assert map_size(deployments) == 2
      assert deployments["d1"].mode == :cluster
      assert deployments["d2"].mode == :single_server
    end
  end

  describe "servers/1" do
    test "returns empty map when no deployments", %{name: name} do
      assert ProcessHistory.servers(name) == %{}
    end

    test "reconstructs servers from deployment_started event", %{name: name} do
      start_deployment(name, "d1",
        servers: %{
          "s1" => %{
            role: :coordinator,
            endpoint: "http://localhost:8529",
            log_file: "/tmp/s1.log"
          },
          "s2" => %{role: :dbserver, endpoint: "http://localhost:8530", log_file: "/tmp/s2.log"}
        }
      )

      %{"d1" => servers} = ProcessHistory.servers(name)
      assert map_size(servers) == 2
      assert servers["s1"].role == :coordinator
      assert servers["s1"].endpoint == "http://localhost:8529"
      assert servers["s1"].log_file == "/tmp/s1.log"
      assert servers["s1"].deployment_id == "d1"
      assert servers["s1"].incarnations == []
    end

    test "tracks incarnations from server_started/stopped events", %{name: name} do
      t1 = ~U[2026-03-09 10:00:00Z]
      t2 = ~U[2026-03-09 10:01:00Z]
      t3 = ~U[2026-03-09 10:02:00Z]
      t4 = ~U[2026-03-09 10:03:00Z]

      start_deployment(name, "d1", servers: %{"s1" => %{role: :coordinator}})

      # First incarnation
      cast(name, %{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t1
      })

      cast(name, %{
        event: :server_stopped,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t2
      })

      # Second incarnation (restart)
      cast(name, %{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 2002,
        timestamp: t3
      })

      cast(name, %{
        event: :server_stopped,
        deployment_id: "d1",
        server_id: "s1",
        pid: 2002,
        timestamp: t4
      })

      %{"d1" => %{"s1" => server}} = ProcessHistory.servers(name)
      assert length(server.incarnations) == 2

      [first, second] = server.incarnations
      assert first.pid == 1001
      assert first.started_at == t1
      assert first.stopped_at == t2
      assert second.pid == 2002
      assert second.started_at == t3
      assert second.stopped_at == t4
    end

    test "crash closes the current incarnation", %{name: name} do
      t1 = ~U[2026-03-09 10:00:00Z]
      t2 = ~U[2026-03-09 10:01:00Z]

      start_deployment(name, "d1", servers: %{"s1" => %{role: :dbserver}})

      cast(name, %{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t1
      })

      cast(name, %{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        crash_info: %{signal: 11},
        expected: false,
        timestamp: t2
      })

      %{"d1" => %{"s1" => server}} = ProcessHistory.servers(name)
      assert [inc] = server.incarnations
      assert inc.stopped_at == t2
    end

    test "server_identified sets arango_id", %{name: name} do
      start_deployment(name, "d1", servers: %{"s1" => %{role: :coordinator}})

      cast(name, %{
        event: :server_identified,
        deployment_id: "d1",
        server_id: "s1",
        arango_id: "CRDN-abc123",
        timestamp: ts()
      })

      %{"d1" => %{"s1" => server}} = ProcessHistory.servers(name)
      assert server.arango_id == "CRDN-abc123"
    end

    test "multiple deployments tracked independently", %{name: name} do
      start_deployment(name, "d1", servers: %{"s1" => %{role: :coordinator}})
      start_deployment(name, "d2", mode: :single_server, servers: %{"s2" => %{role: :single}})

      servers = ProcessHistory.servers(name)
      assert map_size(servers) == 2
      assert Map.has_key?(servers["d1"], "s1")
      assert Map.has_key?(servers["d2"], "s2")
    end
  end

  describe "snapshot/1" do
    test "returns all projections consistent with individual queries", %{name: name} do
      t1 = ~U[2026-03-09 10:00:00Z]
      t2 = ~U[2026-03-09 10:01:00Z]
      t3 = ~U[2026-03-09 10:02:00Z]
      t4 = ~U[2026-03-09 10:03:00Z]

      start_deployment(name, "d1",
        mode: :cluster,
        servers: %{
          "s1" => %{role: :coordinator, endpoint: "http://localhost:8529"}
        }
      )

      cast(name, %{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t1
      })

      cast(name, %{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        crash_info: %{signal: 11},
        expected: false,
        timestamp: t2
      })

      cast(name, %{
        event: :server_identified,
        deployment_id: "d1",
        server_id: "s1",
        arango_id: "CRDN-abc",
        timestamp: t3
      })

      cast(name, %{
        event: :timeout_kill,
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "s1", os_pid: 1001}],
        timestamp: t4
      })

      snapshot = ProcessHistory.snapshot(name)

      assert Map.keys(snapshot) |> Enum.sort() ==
               [
                 :deployments,
                 :events,
                 :pids_by_server,
                 :servers,
                 :timeout_kills,
                 :unexpected_crashes
               ]

      assert snapshot.events == ProcessHistory.events(name)
      assert snapshot.pids_by_server == ProcessHistory.pids_by_server(name)
      assert snapshot.unexpected_crashes == ProcessHistory.unexpected_crashes(name)
      assert snapshot.timeout_kills == ProcessHistory.timeout_kills(name)
      assert snapshot.deployments == ProcessHistory.deployments(name)
      assert snapshot.servers == ProcessHistory.servers(name)
    end
  end

  describe "edge cases" do
    test "deployment_started without prior deployment_starting", %{name: name} do
      ts = ~U[2026-03-09 10:00:00Z]

      cast(name, %{
        event: :deployment_started,
        deployment_id: "d1",
        servers: %{},
        timestamp: ts
      })

      %{"d1" => d} = ProcessHistory.deployments(name)
      assert d.mode == nil
      assert d.stacktrace == nil
      assert d.started_at == ts
    end

    test "deployment_stopped for unknown deployment", %{name: name} do
      ts = ~U[2026-03-09 10:00:00Z]

      cast(name, %{
        event: :deployment_stopped,
        deployment_id: "d_unknown",
        timestamp: ts
      })

      %{"d_unknown" => d} = ProcessHistory.deployments(name)
      assert d.stopped_at == ts
    end

    test "server_identified for unknown server", %{name: name} do
      cast(name, %{
        event: :server_identified,
        deployment_id: "d1",
        server_id: "s_unknown",
        arango_id: "CRDN-xyz",
        timestamp: ts()
      })

      assert ProcessHistory.servers(name) == %{}
    end

    test "server_started without deployment_started", %{name: name} do
      cast(name, %{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: ts()
      })

      assert ProcessHistory.pids_by_server(name) == %{"s1" => [1001]}
      assert ProcessHistory.servers(name) == %{}
    end

    test "server_crashed with expected: true does not appear in unexpected_crashes", %{name: name} do
      t1 = ~U[2026-03-09 10:00:00Z]
      t2 = ~U[2026-03-09 10:01:00Z]

      start_deployment(name, "d1", servers: %{"s1" => %{role: :dbserver}})

      cast(name, %{
        event: :server_started,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        timestamp: t1
      })

      cast(name, %{
        event: :server_crashed,
        deployment_id: "d1",
        server_id: "s1",
        pid: 1001,
        crash_info: %{signal: 15},
        expected: true,
        timestamp: t2
      })

      assert ProcessHistory.unexpected_crashes(name) == []

      %{"d1" => %{"s1" => server}} = ProcessHistory.servers(name)
      assert [inc] = server.incarnations
      assert inc.stopped_at == t2
    end
  end

  # --- Helpers ---

  defp cast(name, event), do: GenServer.cast(name, {:event, event})
  defp ts, do: DateTime.utc_now()

  defp start_deployment(name, did, opts \\ []) do
    mode = Keyword.get(opts, :mode, :cluster)
    servers = Keyword.get(opts, :servers, %{})
    timestamp = Keyword.get(opts, :timestamp, ts())

    cast(name, %{
      event: :deployment_starting,
      deployment_id: did,
      mode: mode,
      stacktrace: Keyword.get(opts, :stacktrace),
      specs: [],
      timestamp: timestamp
    })

    cast(name, %{
      event: :deployment_started,
      deployment_id: did,
      servers: servers,
      timestamp: timestamp
    })
  end
end
