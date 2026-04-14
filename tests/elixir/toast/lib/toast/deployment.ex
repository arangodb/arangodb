defmodule Toast.Deployment do
  @moduledoc "Start and stop ArangoDB deployments for testing."

  require Logger

  alias Toast.Client
  alias Toast.Deployment.{Config, Controller, DefaultEventListener, ServerInstance}

  defmodule ServerInfo do
    @moduledoc "Static, immutable server info snapshot taken at deploy time."

    @type t :: %__MODULE__{
            id: String.t(),
            role: Toast.Deployment.ServerInstance.role(),
            port: non_neg_integer(),
            endpoint: String.t(),
            arango_id: String.t() | nil
          }

    @enforce_keys [:id, :role, :port, :endpoint]
    defstruct [:id, :role, :port, :endpoint, :arango_id]
  end

  @type mode :: :single_server | :cluster

  @typedoc """
  Target specifier for server control operations.

  - `"server-id"` — direct server ID string (e.g., `"single"`, `"dbserver-0"`)
  - `[role: :dbserver]` — all servers with that role
  - `[role: :coordinator, index: 0]` — specific server by role and index
  - `[arango_id: "PRMR-abc"]` — server by ArangoDB-assigned internal ID
  """
  @type server_target ::
          String.t()
          | [role: atom()]
          | [role: atom(), index: non_neg_integer()]
          | [arango_id: String.t()]

  @type t :: %__MODULE__{
          id: String.t(),
          controller: pid() | nil,
          api_version: non_neg_integer() | String.t() | nil,
          servers: %{String.t() => ServerInfo.t()},
          jwt_provider: Toast.JWT.Provider.t() | nil
        }

  @enforce_keys [:id]
  defstruct [:id, :controller, :api_version, :jwt_provider, servers: %{}]

  @doc "Returns true if this is a cluster deployment."
  @spec cluster?(t()) :: boolean()
  def cluster?(%__MODULE__{servers: servers}) do
    Enum.any?(servers, fn {_id, srv} -> ServerInstance.cluster_role?(srv.role) end)
  end

  @doc """
  Default endpoint for the deployment — the first coordinator or single server, ordered by ID.
  """
  @spec default_endpoint(t()) :: String.t() | nil
  def default_endpoint(%__MODULE__{servers: servers}) do
    servers
    |> Enum.filter(fn {_id, srv} -> srv.role in [:coordinator, :single] end)
    |> Enum.sort_by(fn {id, _} -> id end)
    |> case do
      [{_id, srv} | _] -> srv.endpoint
      [] -> nil
    end
  end

  @doc "Start a single-server deployment with defaults from app env."
  @spec start_single_server(Path.t(), keyword()) :: {:ok, t()} | {:error, term()}
  def start_single_server(deployment_dir, opts \\ [])

  def start_single_server(%Config{} = config, deployment_dir) do
    start_single_server(config, deployment_dir, [])
  end

  def start_single_server(deployment_dir, opts) when is_binary(deployment_dir) do
    start(Config.new(), deployment_dir, opts)
  end

  @doc "Start a single-server deployment with explicit config."
  @spec start_single_server(Config.t(), Path.t(), keyword()) :: {:ok, t()} | {:error, term()}
  def start_single_server(%Config{} = config, deployment_dir, opts) do
    config = %{config | cluster: nil}
    start(config, deployment_dir, opts)
  end

  @doc "Start a cluster deployment with defaults from app env."
  @spec start_cluster(Path.t(), keyword()) :: {:ok, t()} | {:error, term()}
  def start_cluster(deployment_dir, opts \\ [])

  def start_cluster(%Config{} = config, deployment_dir) do
    start_cluster(config, deployment_dir, [])
  end

  def start_cluster(deployment_dir, opts) when is_binary(deployment_dir) do
    start(Config.new(cluster: true), deployment_dir, opts)
  end

  @doc "Start a cluster deployment with explicit config."
  @spec start_cluster(Config.t(), Path.t(), keyword()) :: {:ok, t()} | {:error, term()}
  def start_cluster(%Config{} = config, deployment_dir, opts) do
    config =
      if config.cluster == nil,
        do: %{config | cluster: Toast.Deployment.ClusterOpts.new()},
        else: config

    start(config, deployment_dir, opts)
  end

  @doc """
  Start a deployment. The mode is derived from the config:
  `config.cluster == nil` → single server, otherwise → cluster.

  The `opts` keyword list can include non-config keys like `:on_crash`,
  `:on_event`, and `:id` that are forwarded to the controller.
  """
  @spec start(Config.t(), Path.t(), keyword()) :: {:ok, t()} | {:error, term()}
  def start(%Config{} = config, deployment_dir, opts \\ []) do
    mode = Config.mode(config)
    id = Keyword.get_lazy(opts, :id, fn -> generate_id(mode) end)
    Logger.info("Starting #{mode} deployment (deployment_dir=#{deployment_dir})")

    stacktrace = capture_caller_stacktrace()

    listener = Keyword.get(opts, :event_listener, DefaultEventListener)

    jwt_provider = maybe_generate_jwt(config, deployment_dir)

    with {:ok, specs} <- build_specs(mode, config, id, deployment_dir),
         {:ok, pid} <-
           Toast.Deployment.Supervisor.start_controller(
             config: config,
             id: id,
             event_listener: listener,
             jwt_provider: jwt_provider
           ),
         :ok <- Controller.deploy(pid, specs, config.startup_timeout, stacktrace: stacktrace) do
      info = Controller.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.id,
         controller: pid,
         api_version: config.api_version,
         servers: build_server_infos(info),
         jwt_provider: jwt_provider
       }}
    end
  end

  defp maybe_generate_jwt(%Config{authentication: false}, _dir), do: nil

  defp maybe_generate_jwt(%Config{authentication: true, jwt_algorithm: alg}, deployment_dir) do
    File.mkdir_p!(deployment_dir)
    {signer, _path} = Toast.JWT.KeyGen.generate(alg, deployment_dir)
    Toast.JWT.Provider.new(signer)
  end

  defp build_specs(:single_server, config, id, deployment_dir),
    do: Toast.Deployment.Factory.build_single_server(config, id, deployment_dir)

  defp build_specs(:cluster, config, id, deployment_dir),
    do: Toast.Deployment.Factory.build_cluster(config, id, deployment_dir)

  @doc "Generate a unique deployment ID for the given mode."
  @spec generate_id(:single_server | :cluster) :: String.t()
  def generate_id(:single_server), do: "single-#{next_deployment_number()}"
  def generate_id(:cluster), do: "cluster-#{next_deployment_number()}"

  @counter_key {__MODULE__, :deployment_counter}

  @doc false
  def init_counter do
    ref = :atomics.new(1, signed: true)
    :persistent_term.put(@counter_key, ref)
    :ok
  end

  defp next_deployment_number do
    n = :atomics.add_get(:persistent_term.get(@counter_key), 1, 1) - 1
    String.pad_leading(Integer.to_string(n), 2, "0")
  end

  @doc """
  Abort all running deployments by sending SIGABRT to every server.

  Iterates all controllers under the deployment supervisor and sends SIGABRT
  to each running server. This triggers the crash handler (backtrace + coredump)
  in each server process.

  Returns a flat list of maps describing each aborted server:
  `[%{server_id: ..., os_pid: ..., log_file: ...}, ...]`
  """
  @spec abort_all() :: [map()]
  def abort_all do
    Logger.info("Aborting all active deployments")

    Toast.Deployment.Supervisor
    |> DynamicSupervisor.which_children()
    |> Enum.flat_map(fn
      {_id, pid, :worker, _modules} when is_pid(pid) ->
        Controller.abort(pid)

      _ ->
        []
    end)
  end

  @spec stop(t(), keyword()) :: {:ok, map()} | {:error, term(), map()}
  def stop(deployment, opts \\ [])

  def stop(%__MODULE__{controller: nil} = deployment, _opts) do
    Logger.info("Stopping deployment #{deployment.id} (no controller)")
    {:ok, %{servers: %{}, error: nil}}
  end

  def stop(%__MODULE__{controller: pid} = deployment, opts) do
    Logger.info("Stopping deployment #{deployment.id}")

    shutdown_result =
      try do
        case Keyword.fetch(opts, :timeout) do
          {:ok, timeout} -> Controller.shutdown(pid, timeout)
          :error -> Controller.shutdown(pid)
        end
      catch
        :exit, _ -> {:error, :controller_dead}
      end

    stop_info = get_stop_info(pid)
    terminate_controller(pid)
    Logger.info("Deployment #{deployment.id} stopped")

    case shutdown_result do
      :ok -> {:ok, stop_info}
      {:error, reason} -> {:error, reason, stop_info}
    end
  end

  @spec dump_agency(t(), keyword()) :: {:ok, iodata() | nil} | {:error, term()}
  def dump_agency(deployment, opts \\ [])

  def dump_agency(%__MODULE__{controller: nil}, _opts), do: {:error, :controller_dead}

  def dump_agency(%__MODULE__{} = deployment, opts) do
    if cluster?(deployment) do
      timeout = Keyword.get(opts, :timeout, 60_000)

      try do
        {:ok, Controller.dump_agency(deployment.controller, timeout)}
      catch
        :exit, _ -> {:error, :controller_dead}
      end
    else
      {:error, :not_cluster}
    end
  end

  @doc "Query the current deployment status."
  @spec status(t()) :: Controller.status()
  def status(deployment) do
    controller_call(deployment, :get_status, :stopped)
  end

  @doc "Get a specific server's current state."
  @spec server(t(), String.t()) :: {:ok, ServerInfo.t()} | {:error, :not_found}
  def server(%__MODULE__{} = deployment, server_id) do
    case controller_call(deployment, {:get_server, server_id}, {:error, :stopped}) do
      {:error, _} = error -> error
      server_instance -> {:ok, to_server_info(server_instance)}
    end
  end

  @doc "List all servers as immutable info snapshots."
  @spec servers(t()) :: [ServerInfo.t()]
  def servers(%__MODULE__{servers: servers}) do
    Map.values(servers)
  end

  @doc "List servers filtered by role."
  @spec servers(t(), keyword()) :: [ServerInfo.t()]
  def servers(%__MODULE__{servers: servers}, role: role) do
    servers |> Map.values() |> Enum.filter(&(&1.role == role))
  end

  @doc "List all servers as full runtime instances (includes mutable state like pid)."
  @spec server_instances(t()) :: [ServerInstance.t()]
  def server_instances(%__MODULE__{} = deployment) do
    controller_call(deployment, :get_servers, [])
  end

  @spec client(t(), String.t()) :: {:ok, Client.t()} | {:error, :not_found}
  def client(%__MODULE__{} = deployment, server_id) when is_binary(server_id) do
    case Map.fetch(deployment.servers, server_id) do
      {:ok, srv} -> {:ok, build_client(deployment, srv)}
      :error -> {:error, :not_found}
    end
  end

  @doc "Like `client/2`, but raises on error."
  @spec client!(t(), String.t()) :: Client.t()
  def client!(%__MODULE__{} = deployment, server_id) when is_binary(server_id) do
    case client(deployment, server_id) do
      {:ok, c} ->
        c

      {:error, :not_found} ->
        raise ArgumentError,
              "server #{inspect(server_id)} not found in deployment #{deployment.id}"
    end
  end

  @doc "Create a client for the `index`-th server with the given role (default: first)."
  @spec client_for_role(t(), atom(), non_neg_integer()) ::
          {:ok, Client.t()} | {:error, :not_found}
  def client_for_role(%__MODULE__{} = deployment, role, index \\ 0) when is_atom(role) do
    deployment.servers
    |> Enum.filter(fn {_id, srv} -> srv.role == role end)
    |> Enum.sort_by(fn {id, _srv} -> id end)
    |> Enum.at(index)
    |> case do
      nil -> {:error, :not_found}
      {_id, srv} -> {:ok, build_client(deployment, srv)}
    end
  end

  @doc "Like `client_for_role/3`, but raises on error."
  @spec client_for_role!(t(), atom(), non_neg_integer()) :: Client.t()
  def client_for_role!(%__MODULE__{} = deployment, role, index \\ 0) when is_atom(role) do
    case client_for_role(deployment, role, index) do
      {:ok, c} ->
        c

      {:error, :not_found} ->
        raise ArgumentError,
              "no server with role #{inspect(role)} at index #{index} in deployment #{deployment.id}"
    end
  end

  @doc "Get the ArangoDB-assigned internal ID for a toast server ID."
  @spec arango_id(t(), String.t()) :: {:ok, String.t()} | {:error, :not_found}
  def arango_id(%__MODULE__{servers: servers}, toast_id) do
    case Map.fetch(servers, toast_id) do
      {:ok, %{arango_id: id}} when id != nil -> {:ok, id}
      _ -> {:error, :not_found}
    end
  end

  @doc "Get server info by ArangoDB-assigned internal ID."
  @spec server_by_arango_id(t(), String.t()) :: {:ok, ServerInfo.t()} | {:error, :not_found}
  def server_by_arango_id(%__MODULE__{servers: servers}, arango_id) do
    case Enum.find(servers, fn {_, s} -> s.arango_id == arango_id end) do
      {_, server} -> {:ok, server}
      nil -> {:error, :not_found}
    end
  end

  @doc "Create a client for the server with the given ArangoDB-assigned internal ID."
  @spec client_for_arango_id(t(), String.t()) :: {:ok, Client.t()} | {:error, :not_found}
  def client_for_arango_id(%__MODULE__{} = deployment, arango_id) when is_binary(arango_id) do
    case server_by_arango_id(deployment, arango_id) do
      {:ok, srv} -> {:ok, build_client(deployment, srv)}
      {:error, _} = err -> err
    end
  end

  @doc "Like `client_for_arango_id/2`, but raises on error."
  @spec client_for_arango_id!(t(), String.t()) :: Client.t()
  def client_for_arango_id!(%__MODULE__{} = deployment, arango_id) when is_binary(arango_id) do
    case client_for_arango_id(deployment, arango_id) do
      {:ok, c} ->
        c

      {:error, :not_found} ->
        raise ArgumentError,
              "no server with arango_id #{inspect(arango_id)} in deployment #{deployment.id}"
    end
  end

  @doc "Get the deployment error if crashed. Returns nil if healthy."
  @spec deployment_error(t()) :: Controller.deployment_error()
  def deployment_error(%__MODULE__{} = d) do
    case controller_call(d, :get_info, nil) do
      nil -> nil
      info -> info.error
    end
  end

  @spec resolve_target(t(), server_target()) :: {:ok, [String.t()]} | {:error, term()}
  def resolve_target(%__MODULE__{} = deployment, target) do
    controller_call(deployment, {:resolve_target, target}, {:error, :stopped})
  end

  # --- Server control operations ---

  @spec stop_server(t(), server_target()) :: :ok | {:error, term()}
  def stop_server(%__MODULE__{} = d, target), do: controller_call_control(d, :stop_server, target)

  @spec kill_server(t(), server_target()) :: :ok | {:error, term()}
  def kill_server(%__MODULE__{} = d, target), do: controller_call_control(d, :kill_server, target)

  @spec pause_server(t(), server_target()) :: :ok | {:error, term()}
  def pause_server(%__MODULE__{} = d, target),
    do: controller_call_control(d, :pause_server, target)

  @spec resume_server(t(), server_target()) :: :ok | {:error, term()}
  def resume_server(%__MODULE__{} = d, target),
    do: controller_call_control(d, :resume_server, target)

  @spec restart_server(t(), server_target(), keyword()) :: :ok | {:error, term()}
  def restart_server(%__MODULE__{} = d, target, opts \\ []),
    do: controller_call_control(d, :restart_server, target, opts)

  @spec start_server(t(), server_target(), keyword()) :: :ok | {:error, term()}
  def start_server(%__MODULE__{} = d, target, opts \\ []),
    do: controller_call_control(d, :start_server, target, opts)

  @spec expect_crash(t(), String.t(), keyword()) :: :ok | {:error, term()}
  def expect_crash(deployment, server_id, opts \\ [])

  def expect_crash(%__MODULE__{controller: nil}, _server_id, _opts),
    do: {:error, :controller_not_available}

  def expect_crash(%__MODULE__{controller: pid}, server_id, opts) do
    timeout = Keyword.get(opts, :timeout, 30_000)
    Controller.expect_crash(pid, server_id, timeout)
  catch
    :exit, _ -> {:error, :controller_not_available}
  end

  @spec verify_crash(t(), String.t(), keyword()) :: {:ok, map()} | {:error, atom()}
  def verify_crash(deployment, server_id, opts \\ [])

  def verify_crash(%__MODULE__{controller: nil}, _server_id, _opts),
    do: {:error, :controller_not_available}

  def verify_crash(%__MODULE__{controller: pid}, server_id, opts) do
    timeout = Keyword.get(opts, :timeout, 5_000)
    Controller.verify_crash(pid, server_id, timeout)
  catch
    :exit, _ -> {:error, :controller_not_available}
  end

  defp get_stop_info(pid) do
    info = Controller.get_info(pid)
    %{servers: info[:servers] || %{}, error: info[:error]}
  catch
    :exit, _ -> %{servers: %{}, error: nil}
  end

  defp terminate_controller(pid) do
    DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
  catch
    :exit, _ -> :ok
  end

  defp controller_call_control(deployment, op, target, opts \\ [])

  defp controller_call_control(%{controller: nil}, _op, _target, _opts),
    do: {:error, :controller_not_available}

  defp controller_call_control(deployment, op, target, opts) do
    apply(Controller, op, [deployment.controller, target, opts])
  catch
    :exit, _ -> {:error, :controller_not_available}
  end

  defp build_server_infos(info) do
    info
    |> Map.get(:servers, %{})
    |> Map.new(fn {id, server} -> {id, to_server_info(server)} end)
  end

  defp to_server_info(%ServerInstance{} = s) do
    %ServerInfo{
      id: s.id,
      role: s.role,
      port: s.port || 0,
      endpoint: s.endpoint || "",
      arango_id: s.arango_id
    }
  end

  defp build_client(%__MODULE__{jwt_provider: nil} = deployment, srv) do
    Client.new(srv.endpoint, api_version: deployment.api_version)
  end

  defp build_client(%__MODULE__{jwt_provider: provider} = deployment, srv) do
    srv.endpoint
    |> Client.new(api_version: deployment.api_version)
    |> Client.with_auth({:jwt_provider, provider})
  end

  defp controller_call(%__MODULE__{controller: nil}, _function, default), do: default

  defp controller_call(%__MODULE__{controller: pid}, function, default) do
    GenServer.call(pid, function)
  catch
    :exit, _ ->
      Logger.debug(
        "Controller call #{inspect(function)} failed (controller dead), returning default"
      )

      default
  end

  defp capture_caller_stacktrace do
    {:current_stacktrace, stacktrace} = Process.info(self(), :current_stacktrace)

    # Trim framework frames — keep only frames from test/suite code
    stacktrace
    |> Enum.drop_while(fn {mod, _fun, _arity, _loc} ->
      mod_str = Atom.to_string(mod)

      String.starts_with?(mod_str, "Elixir.Toast.Deployment")
    end)
    |> Enum.take(10)
  end
end
