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

defmodule ToastTest.Events do
  @moduledoc """
  Public API for emitting custom events into the test event store.

  Custom events are recorded alongside system lifecycle events (server
  starts/stops, test starts/finishes, etc.) and are interleaved with
  server logs by the `mix toast.analyze detail` task. This makes them
  useful for marking points of interest from test code that should
  show up in failure investigation output.

  All custom events share the shape `%{event: :custom, kind: atom(),
  payload: map(), timestamp: integer()}`. Use `kind` to identify the
  event type at a glance and `payload` for arbitrary structured data.

  ## Example

      ToastTest.Events.custom(:cache_invalidated, %{key: "foo", reason: :ttl})
  """

  alias ToastTest.EventStore

  @reserved_payload_keys [:event, :timestamp, :kind]

  @doc """
  Record a custom event with the given `kind` and optional `payload` map.

  Raises `ArgumentError` if `payload` contains any of the reserved keys
  `:event`, `:timestamp`, or `:kind` — these are owned by the event
  envelope and may not be overridden.
  """
  @spec custom(atom(), map()) :: :ok
  def custom(kind, payload \\ %{}) when is_atom(kind) and is_map(payload) do
    case Enum.filter(@reserved_payload_keys, &Map.has_key?(payload, &1)) do
      [] ->
        EventStore.notify(%{event: :custom, kind: kind, payload: payload})

      reserved ->
        raise ArgumentError,
              "custom event payload contains reserved key(s): " <>
                Enum.map_join(reserved, ", ", &inspect/1)
    end
  end
end
