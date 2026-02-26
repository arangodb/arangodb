defmodule ToastTest.ProcessHistoryTest do
  use ExUnit.Case, async: true

  alias ToastTest.ProcessHistory

  setup do
    # Start with a unique name so tests don't conflict
    name = :"process_history_#{System.unique_integer([:positive])}"
    {:ok, pid} = ProcessHistory.start_link(name: name)
    on_exit(fn -> if Process.alive?(pid), do: GenServer.stop(pid) end)
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
    test "records a server lifecycle event", %{pid: pid, name: name} do
      event = %{type: :started, server_id: "s1", timestamp: DateTime.utc_now()}
      GenServer.cast(name, {:event, event})
      :sys.get_state(pid)

      events = GenServer.call(name, :events)
      assert length(events) == 1
      assert hd(events).server_id == "s1"
      assert hd(events).type == :started
    end

    test "records multiple events in order", %{pid: pid, name: name} do
      t1 = DateTime.utc_now()
      t2 = DateTime.add(t1, 1, :second)
      t3 = DateTime.add(t1, 2, :second)

      GenServer.cast(name, {:event, %{type: :started, server_id: "s1", timestamp: t1}})
      GenServer.cast(name, {:event, %{type: :stopped, server_id: "s1", timestamp: t2}})
      GenServer.cast(name, {:event, %{type: :started, server_id: "s2", timestamp: t3}})
      :sys.get_state(pid)

      events = GenServer.call(name, :events)
      assert length(events) == 3
      assert Enum.at(events, 0).type == :started
      assert Enum.at(events, 0).server_id == "s1"
      assert Enum.at(events, 1).type == :stopped
      assert Enum.at(events, 2).server_id == "s2"
    end
  end

  describe "events are timestamped" do
    test "event timestamps are preserved", %{pid: pid, name: name} do
      now = DateTime.utc_now()
      event = %{type: :crashed, server_id: "s1", os_pid: 12345, timestamp: now}
      GenServer.cast(name, {:event, event})
      :sys.get_state(pid)

      [recorded] = GenServer.call(name, :events)
      assert recorded.timestamp == now
    end
  end

  describe "events/0" do
    test "returns empty list when no events recorded", %{name: name} do
      assert GenServer.call(name, :events) == []
    end

    test "returns events in chronological order", %{pid: pid, name: name} do
      for i <- 1..5 do
        GenServer.cast(name, {:event, %{index: i}})
      end

      :sys.get_state(pid)

      events = GenServer.call(name, :events)
      indices = Enum.map(events, & &1.index)
      assert indices == [1, 2, 3, 4, 5]
    end
  end

  describe "clear/0" do
    test "removes all recorded events", %{pid: pid, name: name} do
      GenServer.cast(name, {:event, %{type: :started, server_id: "s1"}})
      GenServer.cast(name, {:event, %{type: :stopped, server_id: "s1"}})
      :sys.get_state(pid)

      assert length(GenServer.call(name, :events)) == 2

      GenServer.cast(name, :clear)
      :sys.get_state(pid)

      assert GenServer.call(name, :events) == []
    end

    test "events can be recorded after clear", %{pid: pid, name: name} do
      GenServer.cast(name, {:event, %{type: :started}})
      :sys.get_state(pid)
      GenServer.cast(name, :clear)
      :sys.get_state(pid)

      GenServer.cast(name, {:event, %{type: :restarted}})
      :sys.get_state(pid)

      events = GenServer.call(name, :events)
      assert length(events) == 1
      assert hd(events).type == :restarted
    end
  end
end
