defmodule ToastTest.Supervisor do
  @moduledoc false

  use Supervisor

  def start_link(_opts \\ []) do
    Supervisor.start_link(__MODULE__, [], name: __MODULE__)
  end

  @impl true
  def init([]) do
    children = [
      ToastTest.Abort,
      ToastTest.DeploymentRegistry,
      ToastTest.EventStore
    ]

    Supervisor.init(children, strategy: :one_for_one)
  end
end
