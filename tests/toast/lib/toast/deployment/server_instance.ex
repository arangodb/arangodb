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

defmodule Toast.Deployment.ServerInstance do
  @moduledoc "Runtime state of a server instance in a deployment."

  @type role :: :single | :agent | :dbserver | :coordinator
  @type operational_state :: :running | :paused | :stopped | :killed | :crashed

  @type t :: %__MODULE__{
          id: String.t(),
          role: role(),
          port: non_neg_integer() | nil,
          endpoint: String.t() | nil,
          pid: non_neg_integer() | nil,
          log_file: Path.t() | nil,
          server_dir: Path.t() | nil,
          server_pid: pid() | nil,
          health_monitor: pid() | nil,
          operational_state: operational_state() | nil,
          expecting_exit: boolean(),
          launch_spec: Toast.Deployment.Factory.LaunchSpec.t() | nil,
          arango_id: String.t() | nil
        }

  @enforce_keys [:id, :role]
  defstruct [
    :id,
    :role,
    :port,
    :endpoint,
    :pid,
    :log_file,
    :server_dir,
    :server_pid,
    :health_monitor,
    :operational_state,
    :launch_spec,
    :arango_id,
    expecting_exit: false
  ]

  @cluster_roles [:agent, :dbserver, :coordinator]

  @spec cluster_role?(role()) :: boolean()
  def cluster_role?(role), do: role in @cluster_roles

  @doc "Whether this server has crashed unexpectedly (not as part of an expected exit)."
  @spec unexpected_crash?(t()) :: boolean()
  def unexpected_crash?(%__MODULE__{operational_state: :crashed, expecting_exit: false}), do: true
  def unexpected_crash?(%__MODULE__{}), do: false

  @spec derive_cluster_status(Enumerable.t({any(), t()})) :: :ready | :degraded | :failed
  def derive_cluster_status(servers) do
    Enum.reduce(servers, :ready, fn
      _server, :failed ->
        :failed

      {_id, server}, acc ->
        cond do
          unexpected_crash?(server) -> :failed
          server.operational_state == :running -> acc
          true -> :degraded
        end
    end)
  end
end
