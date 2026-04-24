defmodule ToastTest.EventStore do
  @moduledoc """
  ETS-backed event store for recording lifecycle events during test runs.

  Events are maps with an `:event` key and a `:timestamp` key (Unix
  microseconds). Stored in an `ordered_set` ETS table keyed by
  `{timestamp, sequence}` so reads are always in chronological order.

  The store is supervised and runs for the entire lifetime of the application.
  """

  use GenServer

  require Logger

  @table __MODULE__

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts \\ []) do
    GenServer.start_link(__MODULE__, opts, name: Keyword.get(opts, :name, __MODULE__))
  end

  @doc """
  Record an event. If the event has no `:timestamp`, one is added automatically.
  """
  @spec notify(map()) :: :ok
  def notify(%{event: _} = event) do
    ts = Map.get_lazy(event, :timestamp, fn -> Toast.get_timestamp() end)
    event = Map.put(event, :timestamp, ts)
    seq = :erlang.unique_integer([:monotonic])
    log_event(event)
    :ets.insert(@table, {{ts, seq}, event})
    :ok
  end

  @doc "Return all recorded events in chronological order."
  @spec events() :: [map()]
  def events do
    :ets.tab2list(@table) |> Enum.map(&elem(&1, 1))
  end

  @doc "Remove all recorded events."
  @spec clear() :: :ok
  def clear do
    :ets.delete_all_objects(@table)
    :ok
  end

  @doc "Return a map of `%{server_id => [os_pid, ...]}` from `:server_started` events."
  @spec pids_by_server() :: %{String.t() => [non_neg_integer()]}
  def pids_by_server do
    snapshot().pids_by_server
  end

  @doc "Return all unexpected crash events in chronological order."
  @spec unexpected_crashes() :: [map()]
  def unexpected_crashes do
    snapshot().unexpected_crashes
  end

  @doc "Return timeout kill events in chronological order."
  @spec timeout_kills() :: [map()]
  def timeout_kills do
    snapshot().timeout_kills
  end

  @doc "Reconstruct deployment metadata from events."
  @spec deployments() :: %{String.t() => map()}
  def deployments do
    snapshot().deployments
  end

  @doc "Reconstruct server metadata from events, grouped by deployment."
  @spec servers() :: %{String.t() => %{String.t() => map()}}
  def servers do
    snapshot().servers
  end

  @doc """
  Return all projections in a single pass over the event stream.

  Returns `%{events: [...], pids_by_server: %{...}, unexpected_crashes: [...],
  timeout_kills: [...], deployments: %{...}, servers: %{...}}`.
  """
  @spec snapshot() :: map()
  def snapshot do
    events() |> __MODULE__.Projections.build()
  end

  # --- GenServer callbacks ---

  @impl true
  def init(_opts) do
    table = :ets.new(@table, [:ordered_set, :public, :named_table])
    {:ok, %{table: table}}
  end

  @impl true
  def terminate(_reason, %{table: table}) do
    :ets.delete(table)
  end

  # --- Logging ---

  defp log_event(%{event: event} = e) do
    detail =
      e
      |> Map.drop([:event, :timestamp])
      |> Enum.map_join(", ", fn {k, v} -> "#{k}=#{inspect(v)}" end)

    Logger.debug("EventStore: #{event} #{detail}")
  end
end
