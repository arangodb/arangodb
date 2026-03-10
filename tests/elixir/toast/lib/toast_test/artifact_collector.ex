defmodule ToastTest.ArtifactCollector do
  @moduledoc """
  Inventories filesystem artifacts (coredumps, sanitizer logs) for server instances.

  Pure discovery only — does not read or parse file contents.
  """

  alias Toast.Deployment.ServerInstance
  alias Toast.Diagnostics.Coredump

  @type server_artifacts :: %{
          server: ServerInstance.t(),
          coredump_paths: [Path.t()],
          sanitizer_files: [Path.t()]
        }

  @type t :: %{String.t() => server_artifacts()}

  @min_sanitizer_bytes 10

  @spec collect(%{String.t() => ServerInstance.t()}, map()) :: t()
  def collect(servers, pid_history \\ %{}) do
    servers
    |> Enum.filter(fn {_id, server} -> server.server_dir != nil end)
    |> Map.new(fn {id, server} ->
      {id, collect_server_artifacts(server, Map.get(pid_history, id, []))}
    end)
  end

  defp collect_server_artifacts(server, historical_pids) do
    %{
      server: server,
      coredump_paths: discover_coredumps(server, historical_pids),
      sanitizer_files: discover_sanitizer_files(server.server_dir)
    }
  end

  defp discover_coredumps(server, historical_pids) do
    os_pids = merge_pids(server.pid, historical_pids)

    Coredump.discover(server_dir: server.server_dir, os_pids: os_pids)
  end

  defp merge_pids(nil, historical), do: historical
  defp merge_pids(current, historical), do: Enum.uniq([current | historical])

  defp discover_sanitizer_files(server_dir) do
    alubsan = Path.wildcard(Path.join(server_dir, "alubsan.log.*"))
    tsan = Path.wildcard(Path.join(server_dir, "tsan.log.*"))

    (alubsan ++ tsan)
    |> Enum.filter(&(file_size(&1) > @min_sanitizer_bytes))
  end

  defp file_size(path) do
    case File.stat(path) do
      {:ok, %{size: size}} -> size
      _ -> 0
    end
  end
end
