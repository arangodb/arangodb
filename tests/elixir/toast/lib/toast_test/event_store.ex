defmodule ToastTest.EventStore do
  @moduledoc """
  ETS-backed event store for recording lifecycle events during test runs.

  Events are maps with an `:event` key and a `:timestamp` key (Unix
  microseconds). Stored in an `ordered_set` ETS table keyed by
  `{timestamp, sequence}` so reads are always in chronological order.

  Any process can call `notify/1` — if the table doesn't exist, the
  event is silently dropped.
  """

  use GenServer

  require Logger

  @table __MODULE__

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts \\ []) do
    GenServer.start_link(__MODULE__, opts, name: Keyword.get(opts, :name, __MODULE__))
  end

  @doc """
  Record an event. Silently drops if the event store is not running.

  If the event has no `:timestamp`, one is added automatically.
  """
  @spec notify(map()) :: :ok
  def notify(%{event: _} = event) do
    ts = Map.get_lazy(event, :timestamp, fn -> Toast.get_timestamp() end)
    event = Map.put(event, :timestamp, ts)
    seq = :erlang.unique_integer([:monotonic])
    log_event(event)
    :ets.insert(@table, {{ts, seq}, event})
    :ok
  catch
    :error, :badarg -> :ok
  end

  @doc "Return all recorded events in chronological order."
  @spec events() :: [map()]
  def events do
    :ets.tab2list(@table) |> Enum.map(&elem(&1, 1))
  catch
    :error, :badarg -> []
  end

  @doc "Remove all recorded events."
  @spec clear() :: :ok
  def clear do
    :ets.delete_all_objects(@table)
    :ok
  catch
    :error, :badarg -> :ok
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

  @doc """
  Record that servers were killed due to a timeout.
  """
  @spec record_timeout_kill(atom(), String.t(), [map()]) :: :ok
  def record_timeout_kill(source, reason, killed_servers) do
    notify(%{
      event: :timeout_kill,
      source: source,
      reason: reason,
      servers: killed_servers
    })
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
    events() |> build_projections()
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
  catch
    :error, :badarg -> :ok
  end

  # --- Projections ---

  defp build_projections(events) do
    Enum.reduce(
      events,
      %{
        events: events,
        pids_by_server: %{},
        pid_sets: %{},
        unexpected_crashes: [],
        timeout_kills: [],
        deployments: %{},
        servers: %{}
      },
      &process_event/2
    )
    |> finalize()
  end

  defp process_event(%{event: :server_started, server_id: sid, pid: pid} = e, acc)
       when is_integer(pid) do
    acc
    |> collect_pid(sid, pid)
    |> add_incarnation(e)
  end

  defp process_event(%{event: :server_stopped} = e, acc), do: close_incarnation(acc, e)

  defp process_event(%{event: :server_crashed, expected: false} = e, acc) do
    %{acc | unexpected_crashes: [e | acc.unexpected_crashes]}
    |> close_incarnation(e)
  end

  defp process_event(%{event: :server_crashed} = e, acc), do: close_incarnation(acc, e)
  defp process_event(%{event: :server_killed}, acc), do: acc

  defp process_event(%{event: :timeout_kill} = e, acc) do
    %{acc | timeout_kills: [e | acc.timeout_kills]}
  end

  defp process_event(%{event: :deployment_starting, deployment_id: did} = e, acc) do
    deployment_meta = %{
      id: did,
      mode: e[:mode],
      stacktrace: e[:stacktrace],
      started_at: e[:timestamp],
      stopped_at: nil
    }

    acc = %{acc | deployments: Map.put(acc.deployments, did, deployment_meta)}

    # Initialize server entries early so server_started events (which fire
    # before deployment_started) can record incarnations.
    init_servers =
      Map.new(e[:specs] || [], fn spec ->
        {spec.id,
         %{
           id: spec.id,
           deployment_id: did,
           role: spec[:role],
           endpoint: nil,
           log_file: spec[:log_file],
           arango_id: nil,
           incarnations: []
         }}
      end)

    %{acc | servers: Map.update(acc.servers, did, init_servers, &Map.merge(&1, init_servers))}
  end

  defp process_event(%{event: :deployment_started, deployment_id: did} = e, acc) do
    acc =
      case acc.deployments do
        %{^did => _} ->
          acc

        _ ->
          meta = %{
            id: did,
            mode: nil,
            stacktrace: nil,
            started_at: e[:timestamp],
            stopped_at: nil
          }

          %{acc | deployments: Map.put(acc.deployments, did, meta)}
      end

    deployment_servers =
      Map.new(e[:servers] || %{}, fn {sid, spec} ->
        {sid,
         %{
           id: sid,
           deployment_id: did,
           role: spec[:role],
           endpoint: spec[:endpoint],
           log_file: spec[:log_file],
           arango_id: nil,
           incarnations: []
         }}
      end)

    # Merge new server data but preserve incarnations already recorded by
    # server_started events that fired before deployment_started.
    %{
      acc
      | servers:
          Map.update(acc.servers, did, deployment_servers, fn existing ->
            Map.merge(deployment_servers, existing, fn
              _key, _new, old when is_list(old) and old != [] -> old
              _key, new, _old -> new
            end)
          end)
    }
  end

  defp process_event(%{event: :deployment_stopped, deployment_id: did} = e, acc) do
    %{
      acc
      | deployments:
          Map.update(acc.deployments, did, %{id: did, stopped_at: e[:timestamp]}, fn d ->
            %{d | stopped_at: e[:timestamp]}
          end)
    }
  end

  defp process_event(
         %{event: :server_identified, deployment_id: did, server_id: sid, arango_id: arango_id},
         acc
       ) do
    update_server_in(acc, did, sid, fn server -> %{server | arango_id: arango_id} end)
  end

  defp process_event(_, acc), do: acc

  defp collect_pid(acc, sid, pid) do
    seen = Map.get(acc.pid_sets, sid, MapSet.new())

    if MapSet.member?(seen, pid) do
      acc
    else
      %{
        acc
        | pid_sets: Map.put(acc.pid_sets, sid, MapSet.put(seen, pid)),
          pids_by_server: Map.update(acc.pids_by_server, sid, [pid], fn pids -> [pid | pids] end)
      }
    end
  end

  defp add_incarnation(acc, %{deployment_id: did, server_id: sid, pid: pid, timestamp: ts}) do
    update_server_in(acc, did, sid, fn server ->
      %{
        server
        | incarnations: [%{pid: pid, started_at: ts, stopped_at: nil} | server.incarnations]
      }
    end)
  end

  defp add_incarnation(acc, _event), do: acc

  defp close_incarnation(acc, %{deployment_id: did, server_id: sid, pid: pid, timestamp: ts}) do
    update_server_in(acc, did, sid, fn server ->
      %{server | incarnations: close_last_match(server.incarnations, pid, ts)}
    end)
  end

  defp close_incarnation(acc, _event), do: acc

  # Find the last incarnation matching `pid` and set its stopped_at.
  # Incarnations are in reverse order during reduce, so the first match is the latest.
  defp close_last_match(incarnations, pid, ts) do
    close_first_match(incarnations, pid, ts, [])
  end

  defp close_first_match([], _pid, _ts, acc), do: Enum.reverse(acc)

  defp close_first_match([%{pid: pid} = inc | rest], pid, ts, acc) do
    Enum.reverse(acc, [%{inc | stopped_at: ts} | rest])
  end

  defp close_first_match([inc | rest], pid, ts, acc) do
    close_first_match(rest, pid, ts, [inc | acc])
  end

  defp update_server_in(acc, did, sid, update_fn) do
    case acc.servers do
      %{^did => %{^sid => server} = deployment_servers} ->
        %{
          acc
          | servers:
              Map.put(acc.servers, did, Map.put(deployment_servers, sid, update_fn.(server)))
        }

      _ ->
        acc
    end
  end

  defp finalize(acc) do
    %{
      events: acc.events,
      pids_by_server: Map.new(acc.pids_by_server, fn {k, v} -> {k, Enum.reverse(v)} end),
      unexpected_crashes: Enum.reverse(acc.unexpected_crashes),
      timeout_kills: Enum.reverse(acc.timeout_kills),
      deployments: acc.deployments,
      servers: finalize_servers(acc.servers)
    }
  end

  defp finalize_servers(servers) do
    Map.new(servers, fn {did, deployment_servers} ->
      {did,
       Map.new(deployment_servers, fn {sid, server} ->
         {sid, %{server | incarnations: Enum.reverse(server.incarnations)}}
       end)}
    end)
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
