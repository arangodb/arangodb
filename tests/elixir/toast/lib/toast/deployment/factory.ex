defmodule Toast.Deployment.Factory do
  @moduledoc "Build complete server launch specifications from configuration."

  require Logger

  alias Toast.Config
  alias Toast.Deployment.CommandBuilder
  alias Toast.Diagnostics.Sanitizer
  alias Toast.PortAllocator
  alias Toast.Utils.Filesystem

  defmodule LaunchSpec do
    @moduledoc "Server launch specification produced by Factory."

    alias Toast.Deployment.ServerInstance

    @type role :: ServerInstance.role()

    @type t :: %__MODULE__{
            id: String.t(),
            role: role(),
            executable: Path.t(),
            args: [String.t()],
            env: [{String.t(), String.t()}],
            working_dir: Path.t(),
            server_dir: Path.t(),
            port: pos_integer(),
            log_file: Path.t()
          }

    @enforce_keys [
      :id,
      :role,
      :executable,
      :args,
      :env,
      :working_dir,
      :server_dir,
      :port,
      :log_file
    ]
    defstruct [:id, :role, :executable, :args, :env, :working_dir, :server_dir, :port, :log_file]
  end

  @type launch_spec :: LaunchSpec.t()

  @spec build_single_server(Config.t(), String.t()) ::
          {:ok, [launch_spec()]} | {:error, term()}
  def build_single_server(config, server_id) do
    with {:ok, port} <- PortAllocator.allocate(),
         {:ok, executable} <- Filesystem.find_arangod(config.build_dir),
         {:ok, repo_root} <- Filesystem.find_repository_root(config.build_dir),
         {:ok, paths} <- Filesystem.create_server_dirs(config.work_dir, server_id) do
      merged_args =
        config
        |> build_server_args()
        |> Map.merge(role_config_args(config, :single))

      server_spec = %{role: :single, port: port, args: merged_args}
      args = CommandBuilder.build_args(server_spec, paths, repo_root)

      spec = %LaunchSpec{
        id: server_id,
        role: :single,
        executable: executable,
        args: args,
        env:
          Sanitizer.build_env(
            config.active_sanitizers,
            paths.base_dir,
            repo_root,
            config.sanitizer_override
          ),
        # Run from repo root so relative config paths (etc/testing/...) resolve correctly
        working_dir: repo_root,
        server_dir: paths.base_dir,
        port: port,
        log_file: paths.log_file
      }

      Logger.debug(
        "Built single server spec: executable=#{spec.executable} " <>
          "port=#{port} working_dir=#{repo_root} server_dir=#{paths.base_dir}\n" <>
          "  args: #{Enum.join(args, " ")}"
      )

      {:ok, [spec]}
    end
  end

  @spec build_cluster(Config.t(), String.t()) :: {:ok, [launch_spec()]} | {:error, term()}
  def build_cluster(config, deployment_id) do
    total_count =
      config.cluster_agents + config.cluster_dbservers + config.cluster_coordinators

    with {:ok, executable} <- Filesystem.find_arangod(config.build_dir),
         {:ok, repo_root} <- Filesystem.find_repository_root(config.build_dir),
         {:ok, ports} <- PortAllocator.allocate_batch(total_count) do
      {agent_ports, rest} = Enum.split(ports, config.cluster_agents)
      {dbserver_ports, coordinator_ports} = Enum.split(rest, config.cluster_dbservers)

      agency_endpoints = Enum.map(agent_ports, &"tcp://127.0.0.1:#{&1}")

      ctx = %{
        config: config,
        deployment_id: deployment_id,
        executable: executable,
        repo_root: repo_root
      }

      agents =
        build_role_specs(ctx, :agent, agent_ports, &agent_args(config, &1, agency_endpoints))

      dbservers =
        build_role_specs(ctx, :dbserver, dbserver_ports, &dbserver_args(&1, agency_endpoints))

      coordinators =
        build_role_specs(
          ctx,
          :coordinator,
          coordinator_ports,
          &coordinator_args(config, &1, agency_endpoints)
        )

      with {:ok, agents} <- agents,
           {:ok, dbservers} <- dbservers,
           {:ok, coordinators} <- coordinators do
        Logger.debug(
          "Built cluster topology: " <>
            "#{length(agents)} agents, #{length(dbservers)} dbservers, #{length(coordinators)} coordinators"
        )

        {:ok, agents ++ dbservers ++ coordinators}
      end
    end
  end

  defp build_role_specs(ctx, role, ports, custom_args_fn) do
    specs =
      ports
      |> Enum.with_index()
      |> Enum.reduce_while([], fn {port, index}, acc ->
        server_id = "#{ctx.deployment_id}-#{role}-#{index}"

        case build_cluster_server(ctx, server_id, role, port, custom_args_fn.(port)) do
          {:ok, spec} -> {:cont, [spec | acc]}
          {:error, _} = error -> {:halt, error}
        end
      end)

    case specs do
      {:error, _} = error -> error
      list -> {:ok, Enum.reverse(list)}
    end
  end

  defp build_cluster_server(ctx, server_id, role, port, custom_args) do
    %{config: config, executable: executable, repo_root: repo_root} = ctx

    with {:ok, paths} <- Filesystem.create_server_dirs(config.work_dir, server_id) do
      merged_args =
        config
        |> build_server_args()
        |> Map.merge(role_config_args(config, role))
        |> Map.merge(custom_args)

      server_spec = %{role: role, port: port, args: merged_args}
      args = CommandBuilder.build_args(server_spec, paths, repo_root)

      {:ok,
       %LaunchSpec{
         id: server_id,
         role: role,
         executable: executable,
         args: args,
         env:
           Sanitizer.build_env(
             config.active_sanitizers,
             paths.base_dir,
             repo_root,
             config.sanitizer_override
           ),
         working_dir: repo_root,
         server_dir: paths.base_dir,
         port: port,
         log_file: paths.log_file
       }}
    end
  end

  defp agent_args(config, port, agency_endpoints) do
    %{
      "agency.size" => to_string(config.cluster_agents),
      "agency.my-address" => "tcp://127.0.0.1:#{port}",
      "agency.endpoint" => agency_endpoints
    }
  end

  defp dbserver_args(port, agency_endpoints) do
    %{
      "cluster.my-role" => "PRIMARY",
      "cluster.my-address" => "tcp://127.0.0.1:#{port}",
      "cluster.agency-endpoint" => agency_endpoints
    }
  end

  defp coordinator_args(config, port, agency_endpoints) do
    %{
      "cluster.my-role" => "COORDINATOR",
      "cluster.my-address" => "tcp://127.0.0.1:#{port}",
      "cluster.agency-endpoint" => agency_endpoints,
      "cluster.default-replication-factor" => to_string(config.cluster_replication_factor),
      "foxx.force-update-on-startup" => "true"
    }
  end

  defp role_config_args(config, :coordinator), do: config.coordinator_args
  defp role_config_args(config, :dbserver), do: config.dbserver_args
  defp role_config_args(config, :agent), do: config.agent_args
  defp role_config_args(_config, :single), do: %{}

  defp build_server_args(config) do
    base = config.server_args

    if config.show_server_logs do
      # Add stderr log appender — ServerProcess captures stderr and prints it
      Map.put_new(base, "log.output", "+")
    else
      base
    end
  end
end
