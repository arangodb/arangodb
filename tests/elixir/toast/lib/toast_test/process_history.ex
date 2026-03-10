defmodule ToastTest.ProcessHistory do
  @moduledoc "GenServer recording process lifecycle events for post-test diagnostics."

  use GenServer

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts \\ []) do
    name = Keyword.get(opts, :name, __MODULE__)
    GenServer.start_link(__MODULE__, %{}, name: name)
  end

  @spec handle_event(term()) :: :ok
  def handle_event(event) do
    GenServer.cast(__MODULE__, {:event, event})
  end

  @doc false
  @spec events(GenServer.server()) :: [term()]
  def events(server \\ __MODULE__) do
    GenServer.call(server, :events)
  end

  @spec clear() :: :ok
  def clear do
    GenServer.cast(__MODULE__, :clear)
  end

  @doc """
  Return a map of `%{server_id => [os_pid, ...]}` extracted from recorded events.

  Collects OS PIDs from `:server_started` events. Returns an empty map
  if ProcessHistory is not running.
  """
  @spec pids_by_server() :: %{String.t() => [non_neg_integer()]}
  def pids_by_server do
    GenServer.call(__MODULE__, :pids_by_server)
  catch
    :exit, _ -> %{}
  end

  @doc "Return all unexpected crash events in chronological order."
  @spec unexpected_crashes() :: [Toast.Process.CrashEvent.t()]
  def unexpected_crashes do
    GenServer.call(__MODULE__, :unexpected_crashes)
  catch
    :exit, _ -> []
  end

  @impl true
  def init(_) do
    {:ok, %{events: []}}
  end

  @impl true
  def handle_cast({:event, event}, state) do
    {:noreply, %{state | events: [event | state.events]}}
  end

  @impl true
  def handle_cast(:clear, state) do
    {:noreply, %{state | events: []}}
  end

  @impl true
  def handle_call(:events, _from, state) do
    {:reply, Enum.reverse(state.events), state}
  end

  @impl true
  def handle_call(:unexpected_crashes, _from, state) do
    crashes =
      for {:server_crashed, %Toast.Process.CrashEvent{expected: false} = event} <- state.events,
          do: event

    {:reply, Enum.reverse(crashes), state}
  end

  @impl true
  def handle_call(:pids_by_server, _from, state) do
    result =
      state.events
      |> Enum.reduce(%{}, fn
        {:server_started, server_id, os_pid, _timestamp}, acc when is_integer(os_pid) ->
          Map.update(acc, server_id, [os_pid], fn pids ->
            if os_pid in pids, do: pids, else: [os_pid | pids]
          end)

        _other, acc ->
          acc
      end)
      |> Map.new(fn {server_id, pids} -> {server_id, Enum.reverse(pids)} end)

    {:reply, result, state}
  end
end
