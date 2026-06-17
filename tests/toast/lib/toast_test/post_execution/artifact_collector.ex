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

defmodule ToastTest.PostExecution.ArtifactCollector do
  @moduledoc """
  Inventories filesystem artifacts (coredumps, sanitizer logs) for server instances.

  Pure discovery only — does not read or parse file contents.
  """

  alias Toast.Diagnostics.Coredump

  require Logger

  @type server_info :: %{
          :server_dir => Path.t(),
          optional(:log_file) => Path.t() | nil
        }

  @type server_artifacts :: %{
          log_file: Path.t() | nil,
          coredump_paths: [Path.t()],
          sanitizer_files: [Path.t()]
        }

  @type t :: %{String.t() => server_artifacts()}

  @min_sanitizer_bytes 10

  @spec collect(%{String.t() => server_info()}, map(), keyword()) :: t()
  def collect(servers, pid_history \\ %{}, opts \\ []) do
    result =
      servers
      |> Enum.filter(fn {_id, server} -> server.server_dir != nil end)
      |> Task.async_stream(
        fn {id, server} ->
          {id, collect_server_artifacts(id, server, Map.get(pid_history, id, []), opts)}
        end,
        timeout: :infinity,
        ordered: false
      )
      |> Map.new(fn {:ok, result} -> result end)

    Logger.debug(
      "Artifacts: #{coredump_count(result)} coredump(s), #{sanitizer_count(result)} sanitizer report(s) from #{map_size(servers)} server(s)"
    )

    result
  end

  @spec has_coredumps?(t()) :: boolean()
  def has_coredumps?(artifacts), do: coredump_count(artifacts) > 0

  defp coredump_count(artifacts) do
    Enum.sum_by(Map.values(artifacts), &length(&1.coredump_paths))
  end

  defp sanitizer_count(artifacts) do
    Enum.sum_by(Map.values(artifacts), &length(&1.sanitizer_files))
  end

  defp collect_server_artifacts(id, server, historical_pids, opts) do
    %{
      log_file: server.log_file,
      coredump_paths: discover_coredumps(id, server, historical_pids, opts),
      sanitizer_files: discover_sanitizer_files(server.server_dir)
    }
  end

  defp discover_coredumps(id, server, historical_pids, opts) do
    Logger.debug("Discovering coredumps for server #{id} with PIDs #{inspect(historical_pids)}")

    opts
    |> Keyword.take([:coredump_dir, :not_before])
    |> Keyword.merge(server_dir: server.server_dir, os_pids: historical_pids)
    |> Coredump.Discovery.discover()
  end

  defp discover_sanitizer_files(server_dir) do
    alubsan = Path.wildcard(Path.join(server_dir, "alubsan.log.*"))
    tsan = Path.wildcard(Path.join(server_dir, "tsan.log.*"))

    Enum.filter(alubsan ++ tsan, &(file_size(&1) > @min_sanitizer_bytes))
  end

  defp file_size(path) do
    case File.stat(path) do
      {:ok, %{size: size}} -> size
      _ -> 0
    end
  end
end
