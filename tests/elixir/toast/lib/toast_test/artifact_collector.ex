defmodule ToastTest.ArtifactCollector do
  @moduledoc """
  Inventories filesystem artifacts (coredumps, sanitizer logs) for server instances.

  Pure discovery only — does not read or parse file contents.
  """

  alias Toast.Deployment.ServerInstance
  alias Toast.Diagnostics.Coredump

  require Logger

  @type server_artifacts :: %{
          server: ServerInstance.t(),
          coredump_paths: [Path.t()],
          sanitizer_files: [Path.t()]
        }

  @type t :: %{String.t() => server_artifacts()}

  @min_sanitizer_bytes 10

  @spec collect(%{String.t() => ServerInstance.t()}, map(), keyword()) :: t()
  def collect(servers, pid_history \\ %{}, opts \\ []) do
    result =
      servers
      |> Enum.filter(fn {_id, server} -> server.server_dir != nil end)
      |> Task.async_stream(
        fn {id, server} ->
          {id, collect_server_artifacts(server, Map.get(pid_history, id, []), opts)}
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

  defp collect_server_artifacts(server, historical_pids, opts) do
    %{
      server: server,
      coredump_paths: discover_coredumps(server, historical_pids, opts),
      sanitizer_files: discover_sanitizer_files(server.server_dir)
    }
  end

  defp discover_coredumps(server, historical_pids, opts) do
    os_pids = merge_pids(server.pid, historical_pids)
    Logger.debug("Discovering coredumps for server #{server.id} with PIDs #{inspect(os_pids)}")

    opts
    |> Keyword.take([:coredump_dir, :not_before])
    |> Keyword.merge(server_dir: server.server_dir, os_pids: os_pids)
    |> Coredump.Discovery.discover()
  end

  defp merge_pids(nil, historical), do: historical
  defp merge_pids(current, historical), do: Enum.uniq([current | historical])

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
