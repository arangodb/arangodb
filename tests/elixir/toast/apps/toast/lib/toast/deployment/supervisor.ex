defmodule Toast.Deployment.Supervisor do
  @moduledoc false
  use DynamicSupervisor

  @spec start_link(keyword()) :: Supervisor.on_start()
  def start_link(opts \\ []) do
    name = Keyword.get(opts, :name, __MODULE__)
    DynamicSupervisor.start_link(__MODULE__, opts, name: name)
  end

  @doc "Start a SingleServerController under this supervisor."
  @spec start_controller(GenServer.server(), keyword()) :: DynamicSupervisor.on_start_child()
  def start_controller(supervisor \\ __MODULE__, opts) do
    DynamicSupervisor.start_child(supervisor, {Toast.Deployment.SingleServerController, opts})
  end

  @doc "Start a ClusterController under this supervisor."
  @spec start_cluster_controller(GenServer.server(), keyword()) ::
          DynamicSupervisor.on_start_child()
  def start_cluster_controller(supervisor \\ __MODULE__, opts) do
    DynamicSupervisor.start_child(supervisor, {Toast.Deployment.ClusterController, opts})
  end

  @impl true
  def init(_opts) do
    DynamicSupervisor.init(strategy: :one_for_one)
  end
end
