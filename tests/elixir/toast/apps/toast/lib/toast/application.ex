defmodule Toast.Application do
  @moduledoc false

  use Application

  @impl true
  def start(_type, _args) do
    children = [
      {Toast.PortAllocator, []},
      {Toast.Process.Supervisor, []},
      {Toast.Deployment.Supervisor, []}
    ]

    opts = [strategy: :one_for_one, name: Toast.Supervisor]
    Supervisor.start_link(children, opts)
  end
end
