defmodule ToastTest.ProcessHistory do
  @moduledoc """
  GenServer recording deployment and server lifecycle events for post-test diagnostics.

  All events are maps with an `:event` key identifying the event type and a `:timestamp` key.
  The controller calls `notify/1` directly — no callback wiring needed.
  If ProcessHistory is not running, `notify/1` silently drops the event.
  """

  use GenServer

  require Logger

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts \\ []) do
    name = Keyword.get(opts, :name, __MODULE__)
    GenServer.start_link(__MODULE__, %{}, name: name)
  end

  @doc "Record an event. Silently drops if ProcessHistory is not running."
  @spec notify(map()) :: :ok
  def notify(%{event: _} = event) do
    log_event(event)
    GenServer.cast(__MODULE__, {:event, event})
  catch
    :exit, _ -> :ok
  end

  @doc "Return all recorded events in chronological order."
  @spec events(GenServer.server()) :: [map()]
  def events(server \\ __MODULE__) do
    GenServer.call(server, :events)
  catch
    :exit, _ -> []
  end

  @doc "Remove all recorded events."
  @spec clear() :: :ok
  def clear do
    GenServer.cast(__MODULE__, :clear)
  catch
    :exit, _ -> :ok
  end

  @doc """
  Return a map of `%{server_id => [os_pid, ...]}` extracted from `:server_started` events.

  Each server's PID list is in chronological order (first incarnation first).
  """
  @spec pids_by_server(GenServer.server()) :: %{String.t() => [non_neg_integer()]}
  def pids_by_server(server \\ __MODULE__) do
    GenServer.call(server, :pids_by_server)
  catch
    :exit, _ -> %{}
  end

  @doc "Return all unexpected crash events in chronological order."
  @spec unexpected_crashes(GenServer.server()) :: [map()]
  def unexpected_crashes(server \\ __MODULE__) do
    GenServer.call(server, :unexpected_crashes)
  catch
    :exit, _ -> []
  end

  @doc "Return timeout kill events in chronological order."
  @spec timeout_kills(GenServer.server()) :: [map()]
  def timeout_kills(server \\ __MODULE__) do
    GenServer.call(server, :timeout_kills)
  catch
    :exit, _ -> []
  end

  @doc """
  Record that servers were killed due to a timeout.

  `source` identifies what timed out (e.g., `:suite_timeout`, `:global_timeout`).
  `reason` is a human-readable description.
  `killed_servers` is the list returned by `Toast.Deployment.abort_all/0`.
  """
  @spec record_timeout_kill(atom(), String.t(), [map()]) :: :ok
  def record_timeout_kill(source, reason, killed_servers) do
    notify(%{
      event: :timeout_kill,
      source: source,
      reason: reason,
      servers: killed_servers,
      timestamp: DateTime.utc_now()
    })
  end

  @doc """
  Reconstruct deployment metadata from events.

  Returns `%{deployment_id => deployment_meta}`.
  """
  @spec deployments(GenServer.server()) :: %{String.t() => map()}
  def deployments(server \\ __MODULE__) do
    GenServer.call(server, :deployments)
  catch
    :exit, _ -> %{}
  end

  @doc """
  Reconstruct server metadata from events, grouped by deployment.

  Returns `%{deployment_id => %{server_id => server_meta}}`.
  """
  @spec servers(GenServer.server()) :: %{String.t() => %{String.t() => map()}}
  def servers(server \\ __MODULE__) do
    GenServer.call(server, :servers)
  catch
    :exit, _ -> %{}
  end

  @doc """
  Return all projections in a single call, avoiding repeated event list reversals.

  Returns `%{events: [...], pids_by_server: %{...}, unexpected_crashes: [...],
  timeout_kills: [...], deployments: %{...}, servers: %{...}}`.
  """
  @spec snapshot(GenServer.server()) :: map()
  def snapshot(server \\ __MODULE__) do
    GenServer.call(server, :snapshot)
  catch
    :exit, _ ->
      %{
        events: [],
        pids_by_server: %{},
        unexpected_crashes: [],
        timeout_kills: [],
        deployments: %{},
        servers: %{}
      }
  end

  # --- GenServer callbacks ---

  @impl true
  def init(_) do
    {:ok, %{events: []}}
  end

  @impl true
  def handle_cast({:event, event}, state) do
    {:noreply, %{state | events: [event | state.events]}}
  end

  @impl true
  def handle_cast(:clear, _state) do
    {:noreply, %{events: []}}
  end

  @impl true
  def handle_call(:events, _from, state) do
    {:reply, Enum.reverse(state.events), state}
  end

  @impl true
  def handle_call(:pids_by_server, _from, state) do
    {:reply, build_snapshot(state.events).pids_by_server, state}
  end

  @impl true
  def handle_call(:unexpected_crashes, _from, state) do
    {:reply, build_snapshot(state.events).unexpected_crashes, state}
  end

  @impl true
  def handle_call(:timeout_kills, _from, state) do
    {:reply, build_snapshot(state.events).timeout_kills, state}
  end

  @impl true
  def handle_call(:deployments, _from, state) do
    {:reply, build_snapshot(state.events).deployments, state}
  end

  @impl true
  def handle_call(:servers, _from, state) do
    {:reply, build_snapshot(state.events).servers, state}
  end

  @impl true
  def handle_call(:snapshot, _from, state) do
    {:reply, build_snapshot(state.events), state}
  end

  # --- Snapshot: single-pass reconstruction ---

  defp build_snapshot(reversed_events) do
    events = Enum.reverse(reversed_events)

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
    |> finalize_snapshot()
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

    %{acc | deployments: Map.put(acc.deployments, did, deployment_meta)}
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

    %{
      acc
      | servers:
          Map.update(acc.servers, did, deployment_servers, &Map.merge(&1, deployment_servers))
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
          pids_by_server: Map.update(acc.pids_by_server, sid, [pid], fn pids -> pids ++ [pid] end)
      }
    end
  end

  defp add_incarnation(acc, %{deployment_id: did, server_id: sid, pid: pid, timestamp: ts}) do
    update_server_in(acc, did, sid, fn server ->
      %{
        server
        | incarnations: server.incarnations ++ [%{pid: pid, started_at: ts, stopped_at: nil}]
      }
    end)
  end

  defp add_incarnation(acc, _event), do: acc

  defp close_incarnation(acc, %{deployment_id: did, server_id: sid, pid: pid, timestamp: ts}) do
    update_server_in(acc, did, sid, fn server ->
      idx =
        Enum.find_index(Enum.reverse(server.incarnations), fn inc -> inc.pid == pid end)

      if idx do
        real_idx = length(server.incarnations) - 1 - idx

        %{
          server
          | incarnations: List.update_at(server.incarnations, real_idx, &%{&1 | stopped_at: ts})
        }
      else
        server
      end
    end)
  end

  defp close_incarnation(acc, _event), do: acc

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

  defp finalize_snapshot(acc) do
    %{
      events: acc.events,
      pids_by_server: acc.pids_by_server,
      unexpected_crashes: Enum.reverse(acc.unexpected_crashes),
      timeout_kills: Enum.reverse(acc.timeout_kills),
      deployments: acc.deployments,
      servers: acc.servers
    }
  end

  # --- Logging ---

  defp log_event(%{event: :server_started, server_id: sid, pid: pid}),
    do: Logger.debug("ProcessHistory: server_started #{sid} (pid=#{pid})")

  defp log_event(%{event: :server_stopped, server_id: sid}),
    do: Logger.debug("ProcessHistory: server_stopped #{sid}")

  defp log_event(%{event: :server_crashed, server_id: sid}),
    do: Logger.debug("ProcessHistory: server_crashed #{sid}")

  defp log_event(%{event: :server_killed, server_id: sid}),
    do: Logger.debug("ProcessHistory: server_killed #{sid}")

  defp log_event(%{event: :server_paused, server_id: sid}),
    do: Logger.debug("ProcessHistory: server_paused #{sid}")

  defp log_event(%{event: :server_resumed, server_id: sid}),
    do: Logger.debug("ProcessHistory: server_resumed #{sid}")

  defp log_event(%{event: :deployment_starting, deployment_id: did}),
    do: Logger.debug("ProcessHistory: deployment_starting #{did}")

  defp log_event(%{event: :deployment_started, deployment_id: did}),
    do: Logger.debug("ProcessHistory: deployment_started #{did}")

  defp log_event(%{event: :deployment_stopped, deployment_id: did}),
    do: Logger.debug("ProcessHistory: deployment_stopped #{did}")

  defp log_event(%{event: :server_identified, server_id: sid, arango_id: aid}),
    do: Logger.debug("ProcessHistory: server_identified #{sid} => #{aid}")

  defp log_event(%{event: type}),
    do: Logger.debug("ProcessHistory: #{type}")
end
