defmodule Toast.Process.Supervisor do
  @moduledoc false
  use DynamicSupervisor

  @spec start_link(keyword()) :: Supervisor.on_start()
  def start_link(opts \\ []) do
    name = Keyword.get(opts, :name, __MODULE__)
    DynamicSupervisor.start_link(__MODULE__, opts, name: name)
  end

  @doc "Start a ServerProcess under this supervisor."
  @spec start_server(GenServer.server(), keyword()) :: DynamicSupervisor.on_start_child()
  def start_server(supervisor \\ __MODULE__, opts) do
    DynamicSupervisor.start_child(supervisor, {Toast.Process.ServerProcess, opts})
  end

  @doc "Start a HealthMonitor under this supervisor."
  @spec start_health_monitor(GenServer.server(), keyword()) :: DynamicSupervisor.on_start_child()
  def start_health_monitor(supervisor \\ __MODULE__, opts) do
    DynamicSupervisor.start_child(supervisor, {Toast.Process.HealthMonitor, opts})
  end

  @impl true
  def init(_opts) do
    DynamicSupervisor.init(strategy: :one_for_one, max_restarts: 0)
  end
end
