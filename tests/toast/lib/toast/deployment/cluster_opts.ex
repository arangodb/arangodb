################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

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
