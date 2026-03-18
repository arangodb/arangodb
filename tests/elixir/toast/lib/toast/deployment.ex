defmodule Toast.Deployment do
  @moduledoc "Start and stop ArangoDB deployments for testing."

  require Logger

  alias Toast.Client
  alias Toast.Config
  alias Toast.Deployment.{Controller, ServerInstance}

  defmodule ServerInfo do
    @moduledoc "Static, immutable server info snapshot taken at deploy time."

    @type t :: %__MODULE__{
            id: String.t(),
            role: Toast.Deployment.ServerInstance.role(),
            port: non_neg_integer(),
            endpoint: String.t()
          }

    @enforce_keys [:id, :role, :port, :endpoint]
    defstruct [:id, :role, :port, :endpoint]
  end

  @type mode :: :single_server | :cluster

  @typedoc """
  Target specifier for server control operations.

  - `"server-id"` — direct server ID string (e.g., `"single"`, `"dbserver-0"`)
  - `[role: :dbserver]` — all servers with that role
  - `[role: :coordinator, index: 0]` — specific server by role and index
  - `[cluster_id: "PRMR-abc"]` — server by cluster-internal ID
  """
  @type server_target ::
          String.t()
          | [role: atom()]
          | [role: atom(), index: non_neg_integer()]
          | [cluster_id: String.t()]

  @type t :: %__MODULE__{
          id: String.t(),
          controller: pid(),
          api_version: non_neg_integer() | String.t() | nil,
          servers: %{String.t() => ServerInfo.t()}
        }

  @enforce_keys [:id, :controller]
  defstruct [:id, :controller, :api_version, servers: %{}]

  @doc "Returns true if this is a cluster deployment."
  @spec cluster?(t()) :: boolean()
  def cluster?(%__MODULE__{servers: servers}) do
    Enum.any?(servers, fn {_id, srv} -> srv.role in [:agent, :dbserver, :coordinator] end)
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

  @doc "Start a single-server deployment."
  @spec start_single_server(Config.t() | keyword()) :: {:ok, t()} | {:error, term()}
  def start_single_server(config_or_opts \\ [])
  def start_single_server(%Config{} = config), do: start(:single_server, config)
  def start_single_server(opts) when is_list(opts), do: start(:single_server, Config.load(opts))

  @doc "Start a cluster deployment."
  @spec start_cluster(Config.t() | keyword()) :: {:ok, t()} | {:error, term()}
  def start_cluster(config_or_opts \\ [])
  def start_cluster(%Config{} = config), do: start(:cluster, config)
  def start_cluster(opts) when is_list(opts), do: start(:cluster, Config.load(opts))

  @doc """
  Start a deployment with the given mode.

  The `opts` keyword list can include non-config keys like `:on_crash`,
  `:on_event`, and `:id` that are forwarded to the controller.
  """
  @spec start(mode(), Config.t(), keyword()) :: {:ok, t()} | {:error, term()}
  def start(mode, %Config{} = config, opts \\ []) when mode in [:single_server, :cluster] do
    do_start(mode, config, opts)
  end

  defp do_start(mode, config, opts) do
    Logger.info("Starting #{mode} deployment (work_dir=#{config.work_dir})")
    id = Keyword.get_lazy(opts, :id, fn -> generate_id(mode) end)

    with {:ok, specs} <- build_specs(mode, config, id),
         {:ok, pid} <-
           Toast.Deployment.Supervisor.start_controller(
             config: config,
             id: id,
             on_crash: Keyword.get(opts, :on_crash),
             on_event: Keyword.get(opts, :on_event)
           ),
         :ok <- Controller.deploy(pid, specs, config.startup_timeout) do
      info = Controller.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.id,
         controller: pid,
         api_version: config.api_version,
         servers: build_server_infos(info)
       }}
    end
  end

  defp build_specs(:single_server, config, id),
    do: Toast.Deployment.Factory.build_single_server(config, id)

  defp build_specs(:cluster, config, id),
    do: Toast.Deployment.Factory.build_cluster(config, id)

  defp generate_id(:single_server), do: "toast-#{System.unique_integer([:positive])}"
  defp generate_id(:cluster), do: "toast-cluster-#{System.unique_integer([:positive])}"

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
  def stop(%__MODULE__{controller: pid} = deployment, opts \\ []) do
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

  @spec dump_agency(t(), keyword()) :: :ok | {:error, term()}
  def dump_agency(deployment, opts \\ [])

  def dump_agency(%__MODULE__{} = deployment, opts) do
    if cluster?(deployment) do
      timeout = Keyword.get(opts, :timeout, 60_000)

      try do
        Controller.dump_agency(deployment.controller, timeout)
        :ok
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
  @spec server(t(), String.t()) :: {:ok, ServerInstance.t()} | {:error, :not_found}
  def server(%__MODULE__{} = deployment, server_id) do
    case controller_call(deployment, {:get_server, server_id}, {:error, :stopped}) do
      {:error, _} = error -> error
      server_instance -> {:ok, server_instance}
    end
  end

  @doc "List all servers with current state."
  @spec servers(t()) :: [ServerInstance.t()]
  def servers(%__MODULE__{} = deployment) do
    controller_call(deployment, :get_servers, [])
  end

  @doc "List servers filtered by role."
  @spec servers(t(), keyword()) :: [ServerInstance.t()]
  def servers(%__MODULE__{} = deployment, role: role) do
    controller_call(deployment, {:get_servers, role}, [])
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

  @doc "Get the cluster-internal ID for a toast server ID."
  @spec cluster_id(t(), String.t()) :: {:ok, String.t()} | {:error, :not_found | :not_cluster}
  def cluster_id(%__MODULE__{} = deployment, toast_id) do
    if cluster?(deployment) do
      controller_call(deployment, {:cluster_id, toast_id}, {:error, :not_found})
    else
      {:error, :not_cluster}
    end
  end

  @doc "Get server info by cluster-internal ID."
  @spec server_by_cluster_id(t(), String.t()) ::
          {:ok, ServerInstance.t()} | {:error, :not_found | :not_cluster}
  def server_by_cluster_id(%__MODULE__{} = deployment, cluster_internal_id) do
    if cluster?(deployment) do
      controller_call(
        deployment,
        {:server_by_cluster_id, cluster_internal_id},
        {:error, :not_found}
      )
    else
      {:error, :not_cluster}
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

  @doc """
  Check whether the deployment is healthy.

  Checks the controller status. Per-server HTTP health monitoring is handled
  continuously by `Toast.Process.HealthMonitor` — if any server becomes
  unresponsive, the controller is already notified and status set to `:failed`.
  """
  @spec check_health(t(), ExUnit.Test.t() | nil) :: :ok | {:error, String.t()}
  def check_health(%__MODULE__{} = deployment, prev_test \\ nil) do
    case status(deployment) do
      :ready ->
        :ok

      :degraded ->
        {:error, format_degraded_message(deployment, prev_test)}

      :failed ->
        {:error, format_crash_message(deployment_error(deployment))}

      other ->
        {:error, "Deployment not ready (status: #{other})"}
    end
  end

  @spec resolve_target(t(), server_target()) :: {:ok, [String.t()]} | {:error, term()}
  def resolve_target(%__MODULE__{} = deployment, target) do
    controller_call(deployment, {:resolve_target, target}, {:error, :stopped})
  end

  # --- Failure point operations ---

  defdelegate set_failure_point(deployment, target, name),
    to: Toast.Deployment.FailurePoint,
    as: :set

  defdelegate clear_failure_point(deployment, target, name),
    to: Toast.Deployment.FailurePoint,
    as: :clear

  defdelegate clear_all_failure_points(deployment),
    to: Toast.Deployment.FailurePoint,
    as: :clear_all

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
  def expect_crash(%__MODULE__{controller: pid}, server_id, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, 30_000)
    GenServer.call(pid, {:expect_crash, server_id, timeout}, 10_000)
  catch
    :exit, _ -> {:error, :controller_not_available}
  end

  @spec verify_crash(t(), String.t(), keyword()) :: {:ok, map()} | {:error, atom()}
  def verify_crash(%__MODULE__{controller: pid}, server_id, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, 5_000)
    GenServer.call(pid, {:verify_crash, server_id, timeout}, timeout + 5_000)
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

  defp format_crash_message(nil) do
    "Deployment failed (no crash details available)"
  end

  defp format_crash_message({:server_crashed, server_id, crash_info}) do
    "Server crashed (#{server_id}) #{format_crash_exit(crash_info)}"
  end

  defp format_crash_message({:server_unhealthy, server_id}) do
    "Server became unresponsive (#{server_id})"
  end

  defp format_crash_exit(ci) do
    signal_part = if ci.signal, do: " signal=#{ci.signal}", else: ""
    "exit_status=#{ci.exit_status}#{signal_part}"
  end

  defp controller_call_control(deployment, op, target, opts \\ []) do
    apply(Controller, op, [deployment.controller, target | opts_args(opts)])
  catch
    :exit, _ -> {:error, :controller_not_available}
  end

  defp opts_args([]), do: []
  defp opts_args(opts), do: [opts]

  defp format_degraded_message(deployment, prev_test) do
    downed =
      servers(deployment)
      |> Enum.filter(&(&1.operational_state in [:stopped, :killed, :paused]))

    names = Enum.map_join(downed, ", ", & &1.id)
    test_context = format_test_context(prev_test)

    "Deployment is degraded#{test_context} -- " <>
      "servers [#{names}] are still down. " <>
      "Tests must restore all servers before finishing."
  end

  defp format_test_context(nil), do: ""
  defp format_test_context(%{name: name}), do: " after test \"#{name}\""
  defp format_test_context(_), do: ""

  defp build_server_infos(info) do
    info
    |> Map.get(:servers, %{})
    |> Map.new(fn {id, server} ->
      {id,
       %ServerInfo{
         id: server.id,
         role: server.role,
         port: server.port,
         endpoint: server.endpoint
       }}
    end)
  end

  defp build_client(%__MODULE__{} = deployment, srv) do
    Client.new(srv.endpoint, api_version: deployment.api_version)
  end

  defp controller_call(%__MODULE__{controller: pid}, function, default) do
    GenServer.call(pid, function)
  catch
    :exit, _ ->
      Logger.debug(
        "Controller call #{inspect(function)} failed (controller dead), returning default"
      )

      default
  end
end
