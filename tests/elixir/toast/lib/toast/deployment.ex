defmodule Toast.Deployment do
  @moduledoc "Start and stop ArangoDB deployments for testing."

  require Logger

  alias Toast.Client
  alias Toast.Config
  alias Toast.Deployment.{Controller, ServerInstance}

  import Toast.Utils, only: [compact_join: 2]

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
          mode: mode(),
          config: Config.t(),
          controller: pid(),
          endpoint: String.t(),
          work_dir: Path.t()
        }

  @enforce_keys [:id, :mode, :config, :controller, :endpoint, :work_dir]
  defstruct [:id, :mode, :config, :controller, :endpoint, :work_dir]

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

  @doc "Start a deployment with the given mode."
  @spec start(mode(), Config.t()) :: {:ok, t()} | {:error, term()}
  def start(mode, %Config{} = config) when mode in [:single_server, :cluster] do
    do_start(mode, config, [])
  end

  @doc """
  Start a deployment with a pre-loaded config and additional options.

  The `opts` keyword list can include non-config keys like `:on_crash`,
  `:on_event`, and `:id` that are forwarded to the controller.
  """
  @spec start(mode(), Config.t(), keyword()) :: {:ok, t()} | {:error, term()}
  def start(mode, %Config{} = config, opts) when mode in [:single_server, :cluster] do
    do_start(mode, config, opts)
  end

  defp do_start(mode, config, opts) do
    Logger.info("Starting #{mode} deployment (work_dir=#{config.work_dir})")

    controller_opts =
      [
        mode: mode_module(mode),
        config: config,
        on_crash: Keyword.get(opts, :on_crash),
        on_event: Keyword.get(opts, :on_event)
      ] ++ Keyword.take(opts, [:id])

    with {:ok, pid} <- Toast.Deployment.Supervisor.start_controller(controller_opts),
         :ok <- Controller.deploy(pid, config.startup_timeout) do
      info = Controller.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.id,
         mode: mode,
         config: config,
         endpoint: info[:coordinator_endpoint] || info[:primary_endpoint] || "",
         controller: pid,
         work_dir: config.work_dir
       }}
    end
  end

  @spec stop(t(), keyword()) :: :ok | {:error, term()}
  def stop(%__MODULE__{controller: pid} = deployment, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, default_shutdown_timeout(deployment))

    with :ok <- Controller.shutdown(pid, timeout) do
      DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
      :ok
    end
  end

  @doc """
  Stop the deployment and return collected diagnostics.

  Uses a multi-step protocol:
    1. Agency dump (cluster only, pre-shutdown while agents are alive)
    2. Shutdown all server processes
    3. Collect base diagnostics (logs, sanitizer errors) from controller
    4. Coredump analysis (post-shutdown, separate timeout)
    5. Merge all diagnostics

  Returns `{:ok, diagnostics}` on success or `{:error, reason, partial_diagnostics}`
  on shutdown failure (partial diagnostics are still collected when possible).
  """
  @spec stop_and_collect(t(), keyword()) :: {:ok, map()} | {:error, term(), map()}
  def stop_and_collect(%__MODULE__{controller: pid} = deployment, opts \\ []) do
    timeout = Keyword.get(opts, :timeout, default_shutdown_timeout(deployment))

    Logger.debug("Stopping deployment #{deployment.id} and collecting diagnostics")

    agency_dump = capture_pre_shutdown_data(pid, deployment)

    shutdown_result =
      try do
        Controller.shutdown(pid, timeout)
      catch
        :exit, _ -> {:error, :controller_dead}
      end

    case shutdown_result do
      :ok ->
        diagnostics = collect_post_shutdown(pid, deployment, opts, agency_dump)
        terminate_controller(pid)
        Logger.debug("Deployment #{deployment.id} stopped, diagnostics collected")
        {:ok, diagnostics}

      {:error, reason} ->
        Logger.warning("Shutdown failed for #{deployment.id}: #{inspect(reason)}")
        partial = collect_post_shutdown(pid, deployment, opts, agency_dump)
        terminate_controller(pid)
        {:error, reason, partial}
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

  @spec client(t(), String.t() | keyword()) :: {:ok, Client.t()} | {:error, term()}
  def client(%__MODULE__{} = deployment, server_id) when is_binary(server_id) do
    case server(deployment, server_id) do
      {:ok, srv} -> {:ok, Client.new(srv.endpoint)}
      {:error, _} = error -> error
    end
  end

  def client(%__MODULE__{} = deployment, opts) when is_list(opts) do
    case Keyword.pop(opts, :role) do
      {nil, _opts} ->
        {:error, :invalid_target}

      {role, opts} ->
        index = Keyword.get(opts, :index, 0)

        case servers(deployment, role: role) do
          srvs when length(srvs) > index -> {:ok, Client.new(Enum.at(srvs, index).endpoint)}
          _ -> {:error, :unknown_server}
        end
    end
  end

  @doc "Get the cluster-internal ID for a toast server ID."
  @spec cluster_id(t(), String.t()) :: {:ok, String.t()} | {:error, :not_found | :not_cluster}
  def cluster_id(%__MODULE__{mode: :cluster} = deployment, toast_id) do
    controller_call(deployment, {:cluster_id, toast_id}, {:error, :not_found})
  end

  def cluster_id(%__MODULE__{}, _toast_id), do: {:error, :not_cluster}

  @doc "Get server info by cluster-internal ID."
  @spec server_by_cluster_id(t(), String.t()) ::
          {:ok, ServerInstance.t()} | {:error, :not_found | :not_cluster}
  def server_by_cluster_id(%__MODULE__{mode: :cluster} = deployment, cluster_internal_id) do
    controller_call(
      deployment,
      {:server_by_cluster_id, cluster_internal_id},
      {:error, :not_found}
    )
  end

  def server_by_cluster_id(%__MODULE__{}, _cluster_internal_id), do: {:error, :not_cluster}

  @doc "Get crash details if the deployment has failed. Returns :no_crash if healthy."
  @spec crash_info(t()) :: {:ok, map()} | :no_crash
  def crash_info(%__MODULE__{} = d) do
    case controller_call(d, :get_info, nil) do
      nil -> :no_crash
      info -> extract_crash_info(info.error, info.servers)
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
        {:error, format_crash_message(crash_info(deployment))}

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

  @doc "Retrieve diagnostics collected during shutdown."
  @spec diagnostics(t()) :: map() | nil
  def diagnostics(deployment) do
    case controller_call(deployment, :get_info, nil) do
      nil -> nil
      info -> info[:diagnostics]
    end
  end

  defp capture_pre_shutdown_data(pid, deployment) do
    if deployment.mode == :cluster and deployment.config.dump_agency_on_error do
      # 10s per request, 3 requests per agent, plus 5s base buffer
      agency_timeout = deployment.config.cluster_agents * 3 * 10_000 + 5_000

      try do
        Controller.dump_agency(pid, agency_timeout)
      catch
        :exit, _ -> nil
      end
    end
  end

  defp collect_post_shutdown(pid, deployment, opts, agency_dump) do
    info =
      try do
        Controller.get_info(pid)
      catch
        :exit, _ -> nil
      end

    base_diagnostics = if info, do: info[:diagnostics]
    servers = if info, do: info[:servers], else: %{}

    coredump_reports = collect_coredumps(servers, deployment, opts)

    merge_diagnostics(base_diagnostics, agency_dump, coredump_reports)
  end

  defp collect_coredumps(servers, deployment, opts) do
    debugger = resolve_debugger(deployment, opts)

    case debugger do
      :none ->
        []

      {:ok, debugger_module} ->
        coredump_timeout =
          Keyword.get(opts, :coredump_timeout, deployment.config.coredump_timeout)

        pid_history = Keyword.get(opts, :pid_history, %{})
        server_infos = server_info_for_coredumps(servers, pid_history)

        Toast.Diagnostics.Coredump.collect(
          servers: server_infos,
          debugger: debugger_module,
          timeout: coredump_timeout
        )
    end
  end

  defp resolve_debugger(deployment, opts) do
    configured = Keyword.get(opts, :debugger, deployment.config.debugger)

    case configured do
      :gdb -> {:ok, Toast.Diagnostics.Coredump.GDB}
      :lldb -> {:ok, Toast.Diagnostics.Coredump.LLDB}
      :none -> :none
      :auto -> Toast.Diagnostics.Coredump.detect_debugger()
      nil -> Toast.Diagnostics.Coredump.detect_debugger()
      module when is_atom(module) -> {:ok, module}
    end
  end

  defp server_info_for_coredumps(servers, pid_history) when is_map(servers) do
    servers
    |> Map.values()
    |> Enum.map(fn server ->
      binary_path =
        if server.launch_spec, do: server.launch_spec.executable, else: nil

      historical_pids = Map.get(pid_history, server.id, [])
      all_pids = merge_pids(server.pid, historical_pids)

      %{
        id: server.id,
        os_pid: server.pid,
        os_pids: all_pids,
        server_dir: server.server_dir,
        binary_path: binary_path
      }
    end)
    |> Enum.filter(& &1.binary_path)
    |> Enum.filter(fn info -> info.os_pids != [] end)
  end

  defp server_info_for_coredumps(_, _pid_history), do: []

  defp merge_pids(current_pid, historical_pids) do
    pids = if current_pid, do: [current_pid], else: []

    (pids ++ historical_pids)
    |> Enum.uniq()
  end

  defp terminate_controller(pid) do
    try do
      DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
    catch
      :exit, _ -> :ok
    end
  end

  defp merge_diagnostics(base, agency_dump, coredump_reports) do
    %Toast.Diagnostics.Result{
      servers: base || %{},
      agency_dump: agency_dump,
      coredump_reports: coredump_reports || []
    }
  end

  defp extract_crash_info({:server_crashed, server_id, crash_info}, servers) do
    server = if servers, do: servers[server_id]
    log_file = if server, do: server.log_file
    log_report = read_and_parse_log(log_file)

    {:ok,
     %{
       server_id: server_id,
       server_crash_info: crash_info,
       log_report: log_report,
       log_file: log_file
     }}
  end

  defp extract_crash_info(_error, _servers), do: :no_crash

  defp format_crash_message(:no_crash) do
    "Deployment failed (no crash details available)"
  end

  defp format_crash_message({:ok, details}) do
    alias Toast.Diagnostics.LogAnalyzer

    [
      "Server crashed",
      if(details.server_id, do: "(#{details.server_id})"),
      if(details.server_crash_info, do: format_crash_exit(details.server_crash_info)),
      format_crash_log_summary(details.log_report, LogAnalyzer)
    ]
    |> compact_join(" ")
  end

  defp format_crash_exit(ci) do
    signal_part = if ci.signal, do: " signal=#{ci.signal}", else: ""
    "exit_status=#{ci.exit_status}#{signal_part}"
  end

  defp format_crash_log_summary(nil, _), do: nil

  defp format_crash_log_summary(log_report, parser) do
    case parser.format_summary(log_report) do
      "No crash detected" -> nil
      summary -> "- #{summary}"
    end
  end

  defp read_and_parse_log(log_file),
    do: Toast.Diagnostics.LogAnalyzer.parse(log_file)

  defp controller_call_control(deployment, op, target, opts \\ []) do
    case opts do
      [] -> apply(Controller, op, [deployment.controller, target])
      opts -> apply(Controller, op, [deployment.controller, target, opts])
    end
  catch
    :exit, _ -> {:error, :controller_not_available}
  end

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

  defp default_shutdown_timeout(%__MODULE__{mode: :cluster}), do: 60_000
  defp default_shutdown_timeout(%__MODULE__{}), do: 30_000

  defp mode_module(:single_server), do: Controller.SingleServer
  defp mode_module(:cluster), do: Controller.Cluster

  defp controller_call(%__MODULE__{controller: pid}, function, default) do
    GenServer.call(pid, function)
  catch
    :exit, _ -> default
  end
end
