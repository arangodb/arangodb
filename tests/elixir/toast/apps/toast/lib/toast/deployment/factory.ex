defmodule Toast.Deployment.Factory do
  @moduledoc "Build complete server launch specifications from configuration."

  alias Toast.Config
  alias Toast.Utils.Filesystem
  alias Toast.Deployment.CommandBuilder

  @type launch_spec :: %{
          id: String.t(),
          executable: Path.t(),
          args: [String.t()],
          env: [{String.t(), String.t()}],
          working_dir: Path.t(),
          server_dir: Path.t(),
          port: pos_integer(),
          log_file: Path.t()
        }

  @spec build_single_server(Config.t(), String.t(), pos_integer()) ::
          {:ok, launch_spec()} | {:error, term()}
  def build_single_server(config, server_id, port) do
    with {:ok, executable} <- Filesystem.find_arangod(config.bin_dir),
         {:ok, repo_root} <- Filesystem.find_repository_root(config.bin_dir),
         {:ok, paths} <- Filesystem.create_server_dirs(config.work_dir, server_id) do
      server_spec = %{role: :single, port: port, args: build_server_args(config)}
      args = CommandBuilder.build_args(server_spec, paths, repo_root)

      {:ok,
       %{
         id: server_id,
         executable: executable,
         args: args,
         env: [],
         # Run from repo root so relative config paths (etc/testing/...) resolve correctly
         working_dir: repo_root,
         server_dir: paths.base_dir,
         port: port,
         log_file: paths.log_file
       }}
    end
  end

  @spec build_server_args(Config.t()) :: %{String.t() => String.t() | [String.t()]}
  def build_server_args(config) do
    defaults =
      if config.show_server_logs do
        %{"log.output" => "-"}
      else
        %{"log.output" => "-;all=error"}
      end

    Map.merge(defaults, config.server_args)
  end
end
