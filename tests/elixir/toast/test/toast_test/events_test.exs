defmodule ToastTest.EventsTest do
  use ExUnit.Case, async: false

  alias ToastTest.{Events, EventStore}

  setup do
    EventStore.clear()
    on_exit(fn -> EventStore.clear() end)
    :ok
  end

  describe "custom/2" do
    test "records a custom event with kind and payload" do
      assert :ok = Events.custom(:cache_invalidated, %{key: "foo", reason: :ttl})

      [event] = EventStore.events()
      assert event.event == :custom
      assert event.kind == :cache_invalidated
      assert event.payload == %{key: "foo", reason: :ttl}
      assert is_integer(event.timestamp)
    end

    test "payload defaults to an empty map" do
      assert :ok = Events.custom(:something_happened)

      [event] = EventStore.events()
      assert event.kind == :something_happened
      assert event.payload == %{}
    end

    test "kind must be an atom" do
      assert_raise FunctionClauseError, fn ->
        Events.custom("not_an_atom", %{})
      end
    end

    test "payload must be a map" do
      assert_raise FunctionClauseError, fn ->
        Events.custom(:foo, [:not, :a, :map])
      end
    end

    test "rejects payload containing reserved :event key" do
      assert_raise ArgumentError, ~r/reserved/, fn ->
        Events.custom(:foo, %{event: :server_started})
      end
    end

    test "rejects payload containing reserved :timestamp key" do
      assert_raise ArgumentError, ~r/reserved/, fn ->
        Events.custom(:foo, %{timestamp: 123})
      end
    end

    test "rejects payload containing reserved :kind key" do
      assert_raise ArgumentError, ~r/reserved/, fn ->
        Events.custom(:foo, %{kind: :other})
      end
    end

    test "custom events appear in chronological order alongside system events" do
      EventStore.notify(%{event: :server_started, server_id: "s1", pid: 1})
      Events.custom(:checkpoint, %{step: 1})
      EventStore.notify(%{event: :server_stopped, server_id: "s1", pid: 1})

      kinds =
        EventStore.events()
        |> Enum.map(fn
          %{event: :custom, kind: k} -> {:custom, k}
          %{event: e} -> e
        end)

      assert kinds == [:server_started, {:custom, :checkpoint}, :server_stopped]
    end
  end
end
