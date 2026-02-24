defmodule Toast.Deployment do
  @moduledoc "Start and stop ArangoDB deployments for testing."

  require Logger

  alias Toast.Client
  alias Toast.Config
  alias Toast.Deployment.{SingleServerController, ClusterController, ServerInstance}

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

  @spec start(atom(), keyword()) :: {:ok, t()} | {:error, term()}
  def start(mode \\ :single_server, opts \\ [])

  def start(:single_server, opts) do
    config = Config.load(opts)
    Logger.info("Starting single server deployment (work_dir=#{config.work_dir})")

    on_crash = Keyword.get(opts, :on_crash)
    on_event = Keyword.get(opts, :on_event)

    controller_opts =
      [config: config, on_crash: on_crash, on_event: on_event] ++ Keyword.take(opts, [:id])

    with {:ok, pid} <- Toast.Deployment.Supervisor.start_controller(controller_opts),
         :ok <- SingleServerController.deploy(pid, config.startup_timeout) do
      info = SingleServerController.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.server.id,
         mode: :single_server,
         config: config,
         endpoint: info.server.endpoint,
         controller: pid,
         work_dir: config.work_dir
       }}
    end
  end

  def start(:cluster, opts) do
    config = Config.load(opts)
    Logger.info("Starting cluster deployment (work_dir=#{config.work_dir})")

    on_crash = Keyword.get(opts, :on_crash)
    on_event = Keyword.get(opts, :on_event)

    controller_opts =
      [config: config, on_crash: on_crash, on_event: on_event] ++ Keyword.take(opts, [:id])

    with {:ok, pid} <- Toast.Deployment.Supervisor.start_cluster_controller(controller_opts),
         :ok <- ClusterController.deploy(pid, config.startup_timeout) do
      info = ClusterController.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.id,
         mode: :cluster,
         config: config,
         endpoint: info.coordinator_endpoint,
         controller: pid,
         work_dir: config.work_dir
       }}
    end
  end

  def start(mode, _opts) do
    {:error, {:unsupported_mode, mode}}
  end

  @spec stop(t(), keyword()) :: :ok | {:error, term()}
  def stop(%__MODULE__{controller: pid} = deployment, opts \\ []) do
    mod = controller_module(deployment)
    timeout = Keyword.get(opts, :timeout, default_shutdown_timeout(deployment))

    with :ok <- mod.shutdown(pid, timeout) do
      DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
      :ok
    end
  end

  @doc """
  Stop the deployment and return collected diagnostics.

  Diagnostics are collected during shutdown (between server stop and directory cleanup).
  This function retrieves them before terminating the controller process.
  """
  @spec stop_and_collect(t(), keyword()) :: map() | nil
  def stop_and_collect(%__MODULE__{controller: pid} = deployment, opts \\ []) do
    mod = controller_module(deployment)
    timeout = Keyword.get(opts, :timeout, default_shutdown_timeout(deployment))

    Logger.debug("Stopping deployment #{deployment.id} and collecting diagnostics")

    with :ok <- mod.shutdown(pid, timeout) do
      diagnostics = mod.get_info(pid)[:diagnostics]
      DynamicSupervisor.terminate_child(Toast.Deployment.Supervisor, pid)
      Logger.debug("Deployment #{deployment.id} stopped, diagnostics collected")
      diagnostics
    else
      _ -> nil
    end
  end

  @doc "Query the current deployment status."
  @spec status(t()) :: SingleServerController.status()
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

  @doc "Get crash details if the deployment has failed. Returns :no_crash if healthy."
  @spec crash_info(t()) :: {:ok, map()} | :no_crash
  def crash_info(%__MODULE__{mode: :single_server} = d) do
    case controller_call(d, :get_info, nil) do
      nil -> :no_crash
      info -> extract_crash_info(info.error, info.server.log_file)
    end
  end

  def crash_info(%__MODULE__{mode: :cluster} = d) do
    case controller_call(d, :get_info, nil) do
      nil -> :no_crash
      info -> extract_cluster_crash_info(info.error, info.servers)
    end
  end

  @doc """
  Check whether the deployment is healthy.

  Checks the controller status. Per-server HTTP health monitoring is handled
  continuously by `Toast.Process.HealthMonitor` — if any server becomes
  unresponsive, the controller is already notified and status set to `:failed`.
  """
  @spec check_health(t()) :: :ok | {:error, String.t()}
  def check_health(%__MODULE__{} = deployment) do
    case status(deployment) do
      :ready ->
        :ok

      :failed ->
        {:error, format_crash_message(crash_info(deployment))}

      other ->
        {:error, "Deployment not ready (status: #{other})"}
    end
  end

  @doc "Retrieve diagnostics collected during shutdown."
  @spec diagnostics(t()) :: map() | nil
  def diagnostics(deployment) do
    case controller_call(deployment, :get_info, nil) do
      nil -> nil
      info -> info[:diagnostics]
    end
  end

  defp extract_crash_info({:server_crashed, crash_info}, log_file) do
    log_report = read_and_parse_log(log_file)

    {:ok,
     %{
       server_id: nil,
       server_crash_info: crash_info,
       log_report: log_report,
       log_file: log_file
     }}
  end

  defp extract_crash_info(_error, _log_file), do: :no_crash

  defp extract_cluster_crash_info({:server_crashed, server_id, crash_info}, servers) do
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

  defp extract_cluster_crash_info(_error, _servers), do: :no_crash

  defp format_crash_message(:no_crash) do
    "Deployment failed (no crash details available)"
  end

  defp format_crash_message({:ok, details}) do
    alias Toast.Diagnostics.CrashLogParser

    parts = ["Server crashed"]

    parts =
      if details.server_id,
        do: parts ++ ["(#{details.server_id})"],
        else: parts

    parts =
      if details.server_crash_info do
        ci = details.server_crash_info
        signal_part = if ci.signal, do: " signal=#{ci.signal}", else: ""
        parts ++ ["exit_status=#{ci.exit_status}#{signal_part}"]
      else
        parts
      end

    parts =
      if details.log_report do
        summary = CrashLogParser.format_summary(details.log_report)
        if summary != "No crash detected", do: parts ++ ["- #{summary}"], else: parts
      else
        parts
      end

    Enum.join(parts, " ")
  end

  defp read_and_parse_log(nil), do: nil

  defp read_and_parse_log(log_file) do
    case File.read(log_file) do
      {:ok, content} -> Toast.Diagnostics.CrashLogParser.parse(content)
      {:error, _} -> nil
    end
  end

  defp default_shutdown_timeout(%__MODULE__{mode: :cluster}), do: 60_000
  defp default_shutdown_timeout(%__MODULE__{}), do: 30_000

  defp controller_module(%__MODULE__{mode: :single_server}), do: SingleServerController
  defp controller_module(%__MODULE__{mode: :cluster}), do: ClusterController

  defp controller_call(%__MODULE__{controller: pid}, function, default) do
    GenServer.call(pid, function)
  catch
    :exit, _ -> default
  end
end
