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
