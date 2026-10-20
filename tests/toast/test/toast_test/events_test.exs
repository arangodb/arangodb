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
