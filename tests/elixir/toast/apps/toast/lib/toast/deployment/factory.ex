defmodule Toast.Deployment.Factory do
  @moduledoc "Build complete server launch specifications from configuration."

  require Logger

  alias Toast.Config
  alias Toast.PortAllocator
  alias Toast.Utils.Filesystem
  alias Toast.Deployment.CommandBuilder
  alias Toast.Diagnostics.Sanitizer

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
    with {:ok, executable} <- Filesystem.find_arangod(config.build_dir),
         {:ok, repo_root} <- Filesystem.find_repository_root(config.build_dir),
         {:ok, paths} <- Filesystem.create_server_dirs(config.work_dir, server_id) do
      server_spec = %{role: :single, port: port, args: build_server_args(config)}
      args = CommandBuilder.build_args(server_spec, paths, repo_root)

      spec = %{
        id: server_id,
        executable: executable,
        args: args,
        env: Sanitizer.build_env(config.sanitizer, paths.base_dir, repo_root, config.explicit_sanitizer),
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

      {:ok, spec}
    end
  end

  @type cluster_topology :: %{
          agents: [launch_spec()],
          dbservers: [launch_spec()],
          coordinators: [launch_spec()]
        }

  @spec build_cluster(Config.t(), String.t()) :: {:ok, cluster_topology()} | {:error, term()}
  def build_cluster(config, deployment_id) do
    total_count =
      config.cluster_agents + config.cluster_dbservers + config.cluster_coordinators

    with {:ok, executable} <- Filesystem.find_arangod(config.build_dir),
         {:ok, repo_root} <- Filesystem.find_repository_root(config.build_dir),
         {:ok, ports} <- PortAllocator.allocate_batch(total_count) do
      {agent_ports, rest} = Enum.split(ports, config.cluster_agents)
      {dbserver_ports, coordinator_ports} = Enum.split(rest, config.cluster_dbservers)

      agency_endpoints = Enum.map(agent_ports, &"tcp://127.0.0.1:#{&1}")

      agents =
        build_role_specs(
          config, deployment_id, executable, repo_root,
          :agent, agent_ports, &agent_args(config, &1, agency_endpoints)
        )

      dbservers =
        build_role_specs(
          config, deployment_id, executable, repo_root,
          :dbserver, dbserver_ports, &dbserver_args(&1, agency_endpoints)
        )

      coordinators =
        build_role_specs(
          config, deployment_id, executable, repo_root,
          :coordinator, coordinator_ports, &coordinator_args(config, &1, agency_endpoints)
        )

      with {:ok, agents} <- agents,
           {:ok, dbservers} <- dbservers,
           {:ok, coordinators} <- coordinators do
        Logger.debug(
          "Built cluster topology: " <>
            "#{length(agents)} agents, #{length(dbservers)} dbservers, #{length(coordinators)} coordinators"
        )

        {:ok, %{agents: agents, dbservers: dbservers, coordinators: coordinators}}
      end
    end
  end

  defp build_role_specs(config, deployment_id, executable, repo_root, role, ports, custom_args_fn) do
    specs =
      ports
      |> Enum.with_index()
      |> Enum.reduce_while([], fn {port, index}, acc ->
        server_id = "#{deployment_id}-#{role}-#{index}"

        case build_cluster_server(config, server_id, executable, repo_root, role, port, custom_args_fn.(port)) do
          {:ok, spec} -> {:cont, [spec | acc]}
          {:error, _} = error -> {:halt, error}
        end
      end)

    case specs do
      {:error, _} = error -> error
      list -> {:ok, Enum.reverse(list)}
    end
  end

  defp build_cluster_server(config, server_id, executable, repo_root, role, port, custom_args) do
    with {:ok, paths} <- Filesystem.create_server_dirs(config.work_dir, server_id) do
      merged_args = Map.merge(build_server_args(config), custom_args)
      server_spec = %{role: role, port: port, args: merged_args}
      args = CommandBuilder.build_args(server_spec, paths, repo_root)

      {:ok,
       %{
         id: server_id,
         executable: executable,
         args: args,
         env: Sanitizer.build_env(config.sanitizer, paths.base_dir, repo_root, config.explicit_sanitizer),
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
