defmodule Toast.Deployment.Factory do
  @moduledoc "Build complete server launch specifications from configuration."

  require Logger

  alias Toast.Deployment.Config
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
            log_file: Path.t(),
            ssl: boolean()
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
    defstruct [
      :id,
      :role,
      :executable,
      :args,
      :env,
      :working_dir,
      :server_dir,
      :port,
      :log_file,
      ssl: false
    ]
  end

  @type launch_spec :: LaunchSpec.t()

  @spec build_single_server(Config.t(), String.t(), Path.t()) ::
          {:ok, [launch_spec()]} | {:error, term()}
  def build_single_server(config, server_id, deployment_dir) do
    with {:ok, port} <- PortAllocator.allocate(),
         {:ok, executable} <- Filesystem.find_arangod(config.build_dir),
         {:ok, repo_root} <- Filesystem.find_repository_root(config.build_dir),
         {:ok, paths} <- Filesystem.create_server_dirs(deployment_dir) do
      merged_args =
        config
        |> build_server_args(deployment_dir)
        |> Map.merge(role_config_args(config, :single))

      scheme = endpoint_scheme(config)
      server_spec = %{role: :single, port: port, args: merged_args, endpoint_scheme: scheme}
      args = CommandBuilder.build_args(server_spec, paths, repo_root)

      spec = %LaunchSpec{
        id: server_id,
        role: :single,
        executable: executable,
        args: args,
        ssl: config.ssl,
        env:
          Sanitizer.build_env(
            config.active_sanitizers,
            paths.base_dir,
            repo_root,
            config.sanitizer_override
          ) ++ memory_env(config.memory_budget, :single, nil),
        # Run from repo root so relative config paths (etc/testing/...) resolve correctly
        working_dir: repo_root,
        server_dir: paths.base_dir,
        port: port,
        log_file: paths.log_file
      }

      spec = maybe_apply_rr(spec, config)

      Logger.debug(
        "Built single server spec: executable=#{spec.executable} " <>
          "port=#{port} working_dir=#{repo_root} server_dir=#{paths.base_dir}\n" <>
          "  args: #{Enum.join(spec.args, " ")}"
      )

      {:ok, [spec]}
    end
  end

  @spec build_cluster(Config.t(), String.t(), Path.t()) ::
          {:ok, [launch_spec()]} | {:error, term()}
  def build_cluster(config, deployment_id, deployment_dir) do
    cluster = config.cluster

    total_count =
      cluster.agents + cluster.dbservers + cluster.coordinators

    with {:ok, executable} <- Filesystem.find_arangod(config.build_dir),
         {:ok, repo_root} <- Filesystem.find_repository_root(config.build_dir),
         {:ok, ports} <- PortAllocator.allocate_batch(total_count) do
      {agent_ports, rest} = Enum.split(ports, cluster.agents)
      {dbserver_ports, coordinator_ports} = Enum.split(rest, cluster.dbservers)

      scheme = endpoint_scheme(config)
      agency_endpoints = Enum.map(agent_ports, &"#{scheme}://127.0.0.1:#{&1}")

      ctx = %{
        config: config,
        deployment_id: deployment_id,
        deployment_dir: deployment_dir,
        executable: executable,
        repo_root: repo_root,
        endpoint_scheme: scheme
      }

      agents =
        build_role_specs(
          ctx,
          :agent,
          agent_ports,
          &agent_args(config, scheme, &1, agency_endpoints)
        )

      dbservers =
        build_role_specs(
          ctx,
          :dbserver,
          dbserver_ports,
          &cluster_role_args(scheme, "PRIMARY", &1, agency_endpoints)
        )

      coordinators =
        build_role_specs(
          ctx,
          :coordinator,
          coordinator_ports,
          &coordinator_args(config, scheme, &1, agency_endpoints)
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
        dir_name = "#{role}-#{index}"
        server_id = "#{ctx.deployment_id}-#{dir_name}"

        case build_cluster_server(ctx, server_id, dir_name, role, port, custom_args_fn.(port)) do
          {:ok, spec} -> {:cont, [spec | acc]}
          {:error, _} = error -> {:halt, error}
        end
      end)

    case specs do
      {:error, _} = error -> error
      list -> {:ok, Enum.reverse(list)}
    end
  end

  defp build_cluster_server(ctx, server_id, dir_name, role, port, custom_args) do
    %{
      config: config,
      deployment_dir: deployment_dir,
      executable: executable,
      repo_root: repo_root
    } = ctx

    with {:ok, paths} <- Filesystem.create_server_dirs(deployment_dir, dir_name) do
      merged_args =
        config
        |> build_server_args(deployment_dir)
        |> Map.merge(role_config_args(config, role))
        |> Map.merge(custom_args)

      server_spec = %{
        role: role,
        port: port,
        args: merged_args,
        endpoint_scheme: ctx.endpoint_scheme
      }

      args = CommandBuilder.build_args(server_spec, paths, repo_root)

      spec = %LaunchSpec{
        id: server_id,
        role: role,
        executable: executable,
        args: args,
        ssl: config.ssl,
        env:
          Sanitizer.build_env(
            config.active_sanitizers,
            paths.base_dir,
            repo_root,
            config.sanitizer_override
          ) ++ memory_env(config.memory_budget, role, config.cluster),
        working_dir: repo_root,
        server_dir: paths.base_dir,
        port: port,
        log_file: paths.log_file
      }

      {:ok, maybe_apply_rr(spec, config)}
    end
  end

  defp agent_args(config, scheme, port, agency_endpoints) do
    %{
      "agency.size" => to_string(config.cluster.agents),
      "agency.my-address" => "#{scheme}://127.0.0.1:#{port}",
      "agency.endpoint" => agency_endpoints
    }
  end

  defp cluster_role_args(scheme, role, port, agency_endpoints) do
    %{
      "cluster.my-role" => role,
      "cluster.my-address" => "#{scheme}://127.0.0.1:#{port}",
      "cluster.agency-endpoint" => agency_endpoints
    }
  end

  defp coordinator_args(config, scheme, port, agency_endpoints) do
    Map.merge(
      cluster_role_args(scheme, "COORDINATOR", port, agency_endpoints),
      %{
        "cluster.default-replication-factor" => to_string(config.cluster.replication_factor),
        "foxx.force-update-on-startup" => "true"
      }
    )
  end

  defp role_config_args(config, :coordinator), do: config.cluster.coordinator_args
  defp role_config_args(config, :dbserver), do: config.cluster.dbserver_args
  defp role_config_args(config, :agent), do: config.cluster.agent_args
  defp role_config_args(_config, :single), do: %{}

  defp build_server_args(config, deployment_dir) do
    config.server_args
    |> maybe_add_log_output(config)
    |> maybe_add_auth_args(config, deployment_dir)
    |> maybe_add_ssl_args(config)
  end

  defp maybe_add_log_output(args, %{show_server_logs: true}),
    # Add stderr log appender — ServerProcess captures stderr and prints it
    do: Map.put_new(args, "log.output", "+")

  defp maybe_add_log_output(args, _config), do: args

  defp maybe_add_auth_args(args, %{authentication: true}, deployment_dir) do
    # Keyfile content drives algorithm selection in arangod:
    # PEM → ES256, plain text → HS256. No extra server flag is needed.
    args
    |> Map.put("server.authentication", "true")
    |> Map.put("server.jwt-secret-keyfile", Toast.JWT.KeyGen.keyfile_path(deployment_dir))
  end

  defp maybe_add_auth_args(args, _config, _deployment_dir), do: args

  defp maybe_add_ssl_args(args, %{ssl: true}) do
    Map.put(args, "ssl.keyfile", "etc/testing/server.pem")
  end

  defp maybe_add_ssl_args(args, _config), do: args

  defp endpoint_scheme(%{ssl: true}), do: "ssl"
  defp endpoint_scheme(_config), do: "tcp"

  # --- memory budget ---
  # Distribution ratios match the JS test framework (instance-manager.js).

  @memory_env_var "ARANGODB_OVERRIDE_DETECTED_TOTAL_MEMORY"
  @agent_memory_pct 8
  @dbserver_memory_pct 69
  @coordinator_memory_pct 23

  defp memory_env(nil, _role, _cluster), do: []
  defp memory_env(total, :single, _cluster), do: [{@memory_env_var, to_string(total)}]

  defp memory_env(total, role, cluster) do
    per_server =
      case role do
        :agent -> div(total * @agent_memory_pct, 100 * cluster.agents)
        :dbserver -> div(total * @dbserver_memory_pct, 100 * cluster.dbservers)
        :coordinator -> div(total * @coordinator_memory_pct, 100 * cluster.coordinators)
      end

    [{@memory_env_var, to_string(per_server)}]
  end

  # --- rr wrapping ---

  defp maybe_apply_rr(spec, %{rr: nil}), do: spec

  defp maybe_apply_rr(spec, %{rr: roles, rr_path: rr_path}) do
    if spec.role in roles do
      apply_rr(spec, rr_path)
    else
      spec
    end
  end

  defp apply_rr(spec, rr_path) do
    trace_dir = Path.join(spec.server_dir, "rr-trace")
    File.mkdir_p!(trace_dir)

    %{spec | executable: rr_path, args: ["record", "-o", trace_dir, spec.executable | spec.args]}
  end
end
