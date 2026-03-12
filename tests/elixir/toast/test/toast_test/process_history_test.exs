defmodule ToastTest.ProcessHistoryTest do
  use ExUnit.Case, async: true

  alias ToastTest.ProcessHistory

  setup do
    # Start with a unique name so tests don't conflict
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

  describe "handle_event/1" do
    test "records a server lifecycle event", %{name: name} do
      event = %{type: :started, server_id: "s1", timestamp: DateTime.utc_now()}
      GenServer.cast(name, {:event, event})

      events = ProcessHistory.events(name)
      assert length(events) == 1
      assert hd(events).server_id == "s1"
      assert hd(events).type == :started
    end

    test "records multiple events in order", %{name: name} do
      t1 = DateTime.utc_now()
      t2 = DateTime.add(t1, 1, :second)
      t3 = DateTime.add(t1, 2, :second)

      GenServer.cast(name, {:event, %{type: :started, server_id: "s1", timestamp: t1}})
      GenServer.cast(name, {:event, %{type: :stopped, server_id: "s1", timestamp: t2}})
      GenServer.cast(name, {:event, %{type: :started, server_id: "s2", timestamp: t3}})

      events = ProcessHistory.events(name)
      assert length(events) == 3
      assert Enum.at(events, 0).type == :started
      assert Enum.at(events, 0).server_id == "s1"
      assert Enum.at(events, 1).type == :stopped
      assert Enum.at(events, 2).server_id == "s2"
    end
  end

  describe "events are timestamped" do
    test "event timestamps are preserved", %{name: name} do
      now = DateTime.utc_now()
      event = %{type: :crashed, server_id: "s1", os_pid: 12_345, timestamp: now}
      GenServer.cast(name, {:event, event})

      [recorded] = ProcessHistory.events(name)
      assert recorded.timestamp == now
    end
  end

  describe "event recording" do
    test "returns empty list when no events recorded", %{name: name} do
      assert ProcessHistory.events(name) == []
    end

    test "returns events in chronological order", %{name: name} do
      for i <- 1..5 do
        GenServer.cast(name, {:event, %{index: i}})
      end

      events = ProcessHistory.events(name)
      indices = Enum.map(events, & &1.index)
      assert indices == [1, 2, 3, 4, 5]
    end
  end

  describe "pids_by_server/0" do
    test "returns empty map when no events recorded", %{name: name} do
      assert GenServer.call(name, :pids_by_server) == %{}
    end

    test "collects OS PIDs from server_started events", %{name: name} do
      GenServer.cast(name, {:event, {:server_started, "s1", 1001, ~U[2026-03-09 10:00:00Z]}})
      GenServer.cast(name, {:event, {:server_started, "s2", 1002, ~U[2026-03-09 10:00:01Z]}})

      result = GenServer.call(name, :pids_by_server)
      assert result == %{"s1" => [1001], "s2" => [1002]}
    end

    test "collects multiple PIDs for the same server (relaunch)", %{name: name} do
      GenServer.cast(name, {:event, {:server_started, "s1", 1001, ~U[2026-03-09 10:00:00Z]}})
      GenServer.cast(name, {:event, {:server_started, "s1", 1002, ~U[2026-03-09 10:01:00Z]}})

      result = GenServer.call(name, :pids_by_server)
      assert MapSet.new(result["s1"]) == MapSet.new([1001, 1002])
      assert length(result["s1"]) == 2
    end

    test "deduplicates repeated PIDs", %{name: name} do
      GenServer.cast(name, {:event, {:server_started, "s1", 1001, ~U[2026-03-09 10:00:00Z]}})
      GenServer.cast(name, {:event, {:server_started, "s1", 1001, ~U[2026-03-09 10:00:01Z]}})

      result = GenServer.call(name, :pids_by_server)
      assert result == %{"s1" => [1001]}
    end

    test "ignores non-server_started events", %{name: name} do
      GenServer.cast(name, {:event, {:server_stopped, "s1", 1001, nil, ~U[2026-03-09 10:00:00Z]}})
      GenServer.cast(name, {:event, %{type: :some_other_event}})

      assert GenServer.call(name, :pids_by_server) == %{}
    end
  end

  describe "unexpected_crashes/0" do
    test "returns empty list when no events recorded", %{name: name} do
      assert GenServer.call(name, :unexpected_crashes) == []
    end

    test "returns only unexpected crash events", %{name: name} do
      unexpected = %Toast.Process.CrashEvent{
        server_id: "s1",
        crash_info: %Toast.Process.CrashInfo{
          exit_status: 139,
          signal: 11,
          timestamp: ~U[2026-03-09 10:00:00Z]
        },
        expected: false
      }

      expected = %Toast.Process.CrashEvent{
        server_id: "s2",
        crash_info: %Toast.Process.CrashInfo{
          exit_status: 139,
          signal: 11,
          timestamp: ~U[2026-03-09 10:00:00Z]
        },
        expected: true
      }

      GenServer.cast(name, {:event, {:server_crashed, unexpected}})
      GenServer.cast(name, {:event, {:server_crashed, expected}})

      crashes = GenServer.call(name, :unexpected_crashes)
      assert length(crashes) == 1
      assert hd(crashes).server_id == "s1"
    end

    test "returns crashes in chronological order", %{name: name} do
      for i <- 1..3 do
        event = %Toast.Process.CrashEvent{
          server_id: "s#{i}",
          crash_info: %Toast.Process.CrashInfo{
            exit_status: 139,
            signal: 11,
            timestamp: ~U[2026-03-09 10:00:00Z]
          },
          expected: false
        }

        GenServer.cast(name, {:event, {:server_crashed, event}})
      end

      crashes = GenServer.call(name, :unexpected_crashes)
      ids = Enum.map(crashes, & &1.server_id)
      assert ids == ["s1", "s2", "s3"]
    end
  end

  describe "timeout_kills/0" do
    test "returns empty list when no timeout events", %{name: name} do
      assert GenServer.call(name, :timeout_kills) == []
    end

    test "records and retrieves timeout kill events", %{name: name} do
      kill_info = %{
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "s1", os_pid: 1001, log_file: "/tmp/s1.log"}],
        timestamp: ~U[2026-03-09 10:05:00Z]
      }

      GenServer.cast(name, {:event, {:timeout_kill, kill_info}})

      kills = GenServer.call(name, :timeout_kills)
      assert length(kills) == 1
      assert hd(kills).source == :suite_timeout
      assert hd(kills).reason == "Suite timeout exceeded"
      assert length(hd(kills).servers) == 1
    end

    test "returns multiple timeout kills in chronological order", %{name: name} do
      for {source, i} <- Enum.with_index([:suite_timeout, :global_timeout]) do
        kill_info = %{
          source: source,
          reason: "Timeout #{i}",
          servers: [],
          timestamp: DateTime.add(~U[2026-03-09 10:00:00Z], i, :minute)
        }

        GenServer.cast(name, {:event, {:timeout_kill, kill_info}})
      end

      kills = GenServer.call(name, :timeout_kills)
      sources = Enum.map(kills, & &1.source)
      assert sources == [:suite_timeout, :global_timeout]
    end

    test "ignores non-timeout events", %{name: name} do
      GenServer.cast(name, {:event, {:server_started, "s1", 1001, ~U[2026-03-09 10:00:00Z]}})
      GenServer.cast(name, {:event, %{type: :something_else}})

      assert GenServer.call(name, :timeout_kills) == []
    end
  end

  describe "clear/0" do
    test "removes all recorded events", %{name: name} do
      GenServer.cast(name, {:event, %{type: :started, server_id: "s1"}})
      GenServer.cast(name, {:event, %{type: :stopped, server_id: "s1"}})

      assert length(ProcessHistory.events(name)) == 2

      GenServer.cast(name, :clear)

      assert ProcessHistory.events(name) == []
    end

    test "events can be recorded after clear", %{name: name} do
      GenServer.cast(name, {:event, %{type: :started}})
      GenServer.cast(name, :clear)
      GenServer.cast(name, {:event, %{type: :restarted}})

      events = ProcessHistory.events(name)
      assert length(events) == 1
      assert hd(events).type == :restarted
    end
  end
end
