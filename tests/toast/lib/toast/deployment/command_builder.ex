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

defmodule Toast.Deployment.CommandBuilder do
  @moduledoc "Build arangod CLI arguments for different server roles."

  alias Toast.Deployment.ServerInstance

  @type role :: ServerInstance.role()

  @type server_spec :: %{
          role: role(),
          port: pos_integer(),
          args: %{String.t() => term()},
          endpoint_scheme: String.t()
        }

  @type server_paths :: %{
          required(:data_dir) => Path.t(),
          required(:app_dir) => Path.t(),
          required(:log_file) => Path.t(),
          optional(atom()) => term()
        }

  @spec build_args(server_spec(), server_paths(), Path.t()) :: [String.t()]
  def build_args(%{role: role, port: port, args: args} = spec, server_paths, repo_root) do
    endpoint_scheme = Map.get(spec, :endpoint_scheme, "tcp")

    (base_args(role, port, server_paths, repo_root, endpoint_scheme) ++ role_args(role))
    |> Toast.Utils.flatten_opts()
    |> Kernel.++(flatten_custom_args(args))
  end

  defp config_file(:single), do: "etc/testing/arangod-single.conf"
  defp config_file(:agent), do: "etc/testing/arangod-agent.conf"
  defp config_file(:coordinator), do: "etc/testing/arangod-coordinator.conf"
  defp config_file(:dbserver), do: "etc/testing/arangod-dbserver.conf"

  defp role_args(:single) do
    [{"--server.storage-engine", "rocksdb"}]
  end

  defp role_args(:agent) do
    [
      {"--agency.activate", "true"},
      {"--agency.supervision", "true"}
    ]
  end

  defp role_args(role) when role in [:coordinator, :dbserver] do
    [
      {"--cluster.create-waits-for-sync-replication", "false"},
      {"--cluster.write-concern", "1"}
    ]
  end

  defp flatten_custom_args(args) when map_size(args) == 0, do: []

  defp flatten_custom_args(args) do
    args
    |> Enum.sort_by(&elem(&1, 0))
    |> Enum.flat_map(&expand_arg/1)
  end

  defp base_args(role, port, paths, repo_root, endpoint_scheme) do
    [
      {"--configuration", config_file(role)},
      {"--define", "TOP_DIR=#{repo_root}"},
      {"--server.endpoint", "#{endpoint_scheme}://0.0.0.0:#{port}"},
      {"--database.directory", paths.data_dir},
      {"--javascript.app-path", paths.app_dir},
      {"--log.file", paths.log_file},
      {"--log.level", "crash=info"},
      {"--log.use-json-format", "true"},
      {"--log.ids", "true"},
      {"--log.process", "true"}
    ]
  end

  defp expand_arg({_key, nil}), do: []

  defp expand_arg({key, values}) when is_list(values) do
    Enum.flat_map(values, fn val -> ["--#{key}", to_string(val)] end)
  end

  defp expand_arg({key, value}) do
    ["--#{key}", to_string(value)]
  end
end
