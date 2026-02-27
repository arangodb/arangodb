defmodule Toast.Deployment.CommandBuilder do
  @moduledoc "Build arangod CLI arguments for different server roles."

  @type role :: :single | :agent | :coordinator | :dbserver

  @type server_spec :: %{
          role: role(),
          port: pos_integer(),
          args: %{String.t() => term()}
        }

  @type server_paths :: %{
          data_dir: Path.t(),
          app_dir: Path.t(),
          log_file: Path.t()
        }

  @spec build_args(server_spec(), server_paths(), Path.t()) :: [String.t()]
  def build_args(%{role: role, port: port, args: args}, server_paths, repo_root) do
    base_args(role, port, server_paths, repo_root) ++
      role_args(role) ++
      flatten_custom_args(args)
  end

  @spec config_file(role()) :: String.t()
  def config_file(:single), do: "etc/testing/arangod-single.conf"
  def config_file(:agent), do: "etc/testing/arangod-agent.conf"
  def config_file(:coordinator), do: "etc/testing/arangod-coordinator.conf"
  def config_file(:dbserver), do: "etc/testing/arangod-dbserver.conf"

  @spec role_args(role()) :: [String.t()]
  def role_args(:single), do: ["--server.storage-engine", "rocksdb"]
  def role_args(:agent), do: ["--agency.activate", "true", "--agency.supervision", "true"]

  def role_args(role) when role in [:coordinator, :dbserver] do
    ["--cluster.create-waits-for-sync-replication", "false", "--cluster.write-concern", "1"]
  end

  @spec flatten_custom_args(%{String.t() => term()}) :: [String.t()]
  def flatten_custom_args(args) when map_size(args) == 0, do: []

  def flatten_custom_args(args) do
    args
    |> Enum.sort_by(fn {key, _} -> key end)
    |> Enum.flat_map(&expand_arg/1)
  end

  defp base_args(role, port, paths, repo_root) do
    [
      "--configuration",
      config_file(role),
      "--define",
      "TOP_DIR=#{repo_root}",
      "--server.endpoint",
      "tcp://0.0.0.0:#{port}",
      "--database.directory",
      paths.data_dir,
      "--javascript.app-path",
      paths.app_dir,
      "--log.file",
      paths.log_file
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
