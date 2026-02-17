defmodule Toast.Deployment do
  @moduledoc "Start and stop ArangoDB deployments for testing."

  require Logger

  alias Toast.Config
  alias Toast.Deployment.{Controller, ClusterController}

  @type server_info :: %{role: atom(), port: pos_integer(), endpoint: String.t()}

  @type t :: %__MODULE__{
          id: String.t(),
          mode: :single_server | :cluster,
          endpoint: String.t(),
          controller: pid(),
          crash_monitor: pid(),
          servers: %{String.t() => server_info()} | nil
        }

  @enforce_keys [:id, :mode, :endpoint, :controller]
  defstruct [:id, :mode, :endpoint, :controller, :crash_monitor, :servers]

  @spec start(atom(), keyword()) :: {:ok, t()} | {:error, term()}
  def start(mode \\ :single_server, opts \\ [])

  def start(:single_server, opts) do
    config = Config.load(opts)
    Logger.info("Starting single server deployment (work_dir=#{config.work_dir})")
    crash_monitor = spawn_crash_monitor()

    controller_opts = [config: config, crash_monitor: crash_monitor] ++ Keyword.take(opts, [:id])

    with {:ok, pid} <- Toast.Deployment.Supervisor.start_controller(controller_opts),
         :ok <- Controller.deploy(pid, config.startup_timeout) do
      info = Controller.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.id,
         mode: :single_server,
         endpoint: info.endpoint,
         controller: pid,
         crash_monitor: crash_monitor
       }}
    else
      {:error, _reason} = error ->
        stop_crash_monitor(crash_monitor)
        error
    end
  end

  def start(:cluster, opts) do
    config = Config.load(opts)
    Logger.info("Starting cluster deployment (work_dir=#{config.work_dir})")
    crash_monitor = spawn_crash_monitor()

    controller_opts = [config: config, crash_monitor: crash_monitor] ++ Keyword.take(opts, [:id])

    with {:ok, pid} <- Toast.Deployment.Supervisor.start_cluster_controller(controller_opts),
         :ok <- ClusterController.deploy(pid, config.startup_timeout) do
      info = ClusterController.get_info(pid)

      {:ok,
       %__MODULE__{
         id: info.id,
         mode: :cluster,
         endpoint: info.coordinator_endpoint,
         controller: pid,
         crash_monitor: crash_monitor,
         servers: info.servers
       }}
    else
      {:error, _reason} = error ->
        stop_crash_monitor(crash_monitor)
        error
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
      stop_crash_monitor(deployment.crash_monitor)
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
      stop_crash_monitor(deployment.crash_monitor)
      Logger.debug("Deployment #{deployment.id} stopped, diagnostics collected")
      diagnostics
    else
      _ -> nil
    end
  end

  @doc "Query the current deployment status."
  @spec status(t()) :: Controller.status()
  def status(deployment) do
    controller_call(deployment, :get_status, :stopped)
  end

  @doc "Get crash details if the deployment has failed. Returns :no_crash if healthy."
  @spec crash_info(t()) :: {:ok, map()} | :no_crash
  def crash_info(%__MODULE__{mode: :single_server} = d) do
    case controller_call(d, :get_info, nil) do
      nil -> :no_crash
      info -> extract_crash_info(info.error, info.log_file)
    end
  end

  def crash_info(%__MODULE__{mode: :cluster} = d) do
    case controller_call(d, :get_info, nil) do
      nil -> :no_crash
      info -> extract_cluster_crash_info(info.error, info.servers)
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
    log_file = get_in(servers, [server_id, :log_file])
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

  defp read_and_parse_log(nil), do: nil

  defp read_and_parse_log(log_file) do
    case File.read(log_file) do
      {:ok, content} -> Toast.Diagnostics.CrashLogParser.parse(content)
      {:error, _} -> nil
    end
  end

  defp spawn_crash_monitor do
    spawn(fn ->
      Process.flag(:trap_exit, true)
      crash_monitor_loop()
    end)
  end

  defp crash_monitor_loop do
    receive do
      {:server_crashed, id, info} ->
        Toast.Runner.abort!("Server crashed: #{id}")
        Process.flag(:trap_exit, false)
        exit({:server_crashed, id, info})

      {:EXIT, _pid, _reason} ->
        crash_monitor_loop()
    end
  end

  defp stop_crash_monitor(pid) when is_pid(pid), do: Process.exit(pid, :kill)
  defp stop_crash_monitor(_), do: :ok

  defp default_shutdown_timeout(%__MODULE__{mode: :cluster}), do: 60_000
  defp default_shutdown_timeout(%__MODULE__{}), do: 30_000

  defp controller_module(%__MODULE__{mode: :single_server}), do: Controller
  defp controller_module(%__MODULE__{mode: :cluster}), do: ClusterController

  defp controller_call(%__MODULE__{controller: pid} = deployment, function, default) do
    apply(controller_module(deployment), function, [pid])
  catch
    :exit, _ -> default
  end
end
