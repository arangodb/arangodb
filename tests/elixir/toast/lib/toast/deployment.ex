defmodule Toast.Deployment do
  @moduledoc "Start and stop ArangoDB deployments for testing."

  require Logger

  alias Toast.Client
  alias Toast.Config
  alias Toast.Deployment.{Controller, ServerInstance}

  @type server_target ::
          String.t()
          | [role: atom()]
          | [role: atom(), index: non_neg_integer()]
          | [cluster_id: String.t()]

  @type t :: %__MODULE__{
          id: String.t(),
          mode: :single_server | :cluster,
          config: Config.t(),
          controller: pid(),
          endpoint: String.t(),
          work_dir: Path.t()
        }

  @enforce_keys [:id, :mode, :config, :controller, :endpoint, :work_dir]
  defstruct [:id, :mode, :config, :controller, :endpoint, :work_dir]

  @spec start(atom(), Config.t() | keyword()) :: {:ok, t()} | {:error, term()}
  def start(mode \\ :single_server, config_or_opts \\ [])

  def start(mode, %Config{} = config) when mode in [:single_server, :cluster] do
    do_start(mode, config, [])
  end

  def start(mode, opts) when mode in [:single_server, :cluster] and is_list(opts) do
    do_start(mode, Config.load(opts), opts)
  end

  def start(mode, _config_or_opts) do
    {:error, {:unsupported_mode, mode}}
  end

  @doc """
  Start a deployment with a pre-loaded config and additional options.

  The `opts` keyword list can include non-config keys like `:on_crash`,
  `:on_event`, and `:id` that are forwarded to the controller.
  """
  @spec start(atom(), Config.t(), keyword()) :: {:ok, t()} | {:error, term()}
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

    agency_dump =
      if deployment.mode == :cluster and agency_dump_enabled?(deployment) do
        agency_timeout = deployment.config.cluster_agents * 3 * 10_000 + 5_000

        try do
          Controller.dump_agency(pid, agency_timeout)
        catch
          :exit, _ -> nil
        end
      end

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

  @doc "Get the primary endpoint URL."
  @spec endpoint(t()) :: String.t()
  def endpoint(%__MODULE__{endpoint: ep}), do: ep

  @spec client(t(), String.t() | keyword()) :: {:ok, Client.t()} | {:error, term()}
  def client(%__MODULE__{} = deployment, server_id) when is_binary(server_id) do
    case server(deployment, server_id) do
      {:ok, srv} -> {:ok, Client.new(srv.endpoint)}
      {:error, _} = error -> error
    end
  end

  def client(%__MODULE__{} = deployment, opts) when is_list(opts) do
    if Keyword.has_key?(opts, :role) do
      role = Keyword.fetch!(opts, :role)
      index = Keyword.get(opts, :index, 0)

      case servers(deployment, role: role) do
        [] -> {:error, :unknown_server}
        srvs when length(srvs) > index -> {:ok, Client.new(Enum.at(srvs, index).endpoint)}
        _ -> {:error, :unknown_server}
      end
    else
      {:error, :invalid_target}
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

  defp collect_post_shutdown(pid, deployment, opts, agency_dump) do
    base_diagnostics =
      try do
        Controller.get_info(pid)[:diagnostics]
      catch
        :exit, _ -> nil
      end

    coredump_reports = collect_coredumps(pid, deployment, opts)

    merge_diagnostics(base_diagnostics, agency_dump, coredump_reports)
  end

  defp collect_coredumps(pid, deployment, opts) do
    debugger = resolve_debugger(deployment, opts)

    case debugger do
      :none ->
        []

      {:ok, debugger_module} ->
        coredump_timeout =
          Keyword.get(opts, :coredump_timeout, deployment.config.coredump_timeout)

        servers = get_server_info_for_coredumps(pid)

        Toast.Diagnostics.Coredump.collect(
          servers: servers,
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

  defp get_server_info_for_coredumps(pid) do
    try do
      info = Controller.get_info(pid)

      server_list =
        case info do
          %{servers: servers} when is_map(servers) -> Map.values(servers)
          _ -> []
        end

      server_list
      |> Enum.filter(& &1.pid)
      |> Enum.map(fn server ->
        binary_path =
          if server.launch_spec, do: server.launch_spec.executable, else: nil

        %{
          id: server.id,
          os_pid: server.pid,
          server_dir: server.server_dir,
          binary_path: binary_path
        }
      end)
      |> Enum.filter(& &1.binary_path)
    catch
      :exit, _ -> []
    end
  end

  defp terminate_controller(pid) do
    try do
      DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
    catch
      :exit, _ -> :ok
    end
  end

  defp agency_dump_enabled?(deployment) do
    deployment.config.dump_agency_on_error
  end

  defp merge_diagnostics(base, agency_dump, coredump_reports) do
    optional =
      [
        if(agency_dump, do: {:agency_dump, agency_dump}),
        if(coredump_reports != [], do: {:coredump_reports, coredump_reports})
      ]
      |> Enum.reject(&is_nil/1)
      |> Map.new()

    Map.merge(base || %{}, optional)
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
    alias Toast.Diagnostics.CrashLogParser

    [
      "Server crashed",
      if(details.server_id, do: "(#{details.server_id})"),
      if(details.server_crash_info, do: format_crash_exit(details.server_crash_info)),
      format_crash_log_summary(details.log_report, CrashLogParser)
    ]
    |> Enum.reject(&is_nil/1)
    |> Enum.join(" ")
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

  defp read_and_parse_log(nil), do: nil

  defp read_and_parse_log(log_file) do
    case File.read(log_file) do
      {:ok, content} -> Toast.Diagnostics.CrashLogParser.parse(content)
      {:error, _} -> nil
    end
  end

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
