defmodule Toast.Deployment.ClusterOpts do
  @moduledoc "Cluster topology and per-role arguments for cluster deployments."

  @type t :: %__MODULE__{
          agents: pos_integer(),
          dbservers: pos_integer(),
          coordinators: pos_integer(),
          replication_factor: pos_integer(),
          coordinator_args: %{String.t() => String.t() | [String.t()]},
          dbserver_args: %{String.t() => String.t() | [String.t()]},
          agent_args: %{String.t() => String.t() | [String.t()]}
        }

  defstruct agents: 3,
            dbservers: 3,
            coordinators: 1,
            replication_factor: 2,
            coordinator_args: %{},
            dbserver_args: %{},
            agent_args: %{}

  @doc """
  Build cluster opts from application env, with optional overrides.

  Accepts a keyword list or `true` (use all defaults from app env).
  """
  @spec new(keyword() | true) :: t()
  def new(overrides \\ [])

  def new(true), do: new([])

  def new(overrides) when is_list(overrides) do
    base = %__MODULE__{
      agents: get(:cluster_agents, 3),
      dbservers: get(:cluster_dbservers, 3),
      coordinators: get(:cluster_coordinators, 1),
      replication_factor: get(:cluster_replication_factor, 2),
      coordinator_args: get(:coordinator_args, %{}),
      dbserver_args: get(:dbserver_args, %{}),
      agent_args: get(:agent_args, %{})
    }

    struct!(base, overrides)
  end

  defp get(key, default), do: Application.get_env(:toast, key, default)
end
