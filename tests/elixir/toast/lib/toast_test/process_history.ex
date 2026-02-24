defmodule ToastTest.ProcessHistory do
  use GenServer

  def start_link(opts \\ []) do
    name = Keyword.get(opts, :name, __MODULE__)
    GenServer.start_link(__MODULE__, %{}, name: name)
  end

  def handle_event(event) do
    GenServer.cast(__MODULE__, {:event, event})
  end

  def events do
    GenServer.call(__MODULE__, :events)
  end

  def clear do
    GenServer.cast(__MODULE__, :clear)
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
end
