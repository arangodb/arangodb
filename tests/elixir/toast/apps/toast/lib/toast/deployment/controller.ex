defmodule Toast.Deployment.Controller do
  @moduledoc "GenServer orchestrating the lifecycle of one ArangoDB deployment."

  use GenServer

  require Logger

  alias Toast.Config
  alias Toast.Process.ServerProcess
  alias Toast.Deployment.{Factory, Health}
  alias Toast.Diagnostics.{CrashLogParser, Sanitizer, ServerLog}
  alias Toast.PortAllocator

  @type status :: :stopped | :starting | :ready | :stopping | :failed

  # --- Client API ---

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts) do
    {name, init_opts} = Keyword.pop(opts, :name)

    if name do
      GenServer.start_link(__MODULE__, init_opts, name: name)
    else
      GenServer.start_link(__MODULE__, init_opts)
    end
  end

  @spec deploy(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def deploy(server, timeout \\ 60_000) do
    GenServer.call(server, {:deploy, timeout}, timeout + 5_000)
  end

  @spec shutdown(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def shutdown(server, timeout \\ 30_000) do
    GenServer.call(server, {:shutdown, timeout}, timeout + 5_000)
  end

  @spec get_status(GenServer.server()) :: status()
  def get_status(server) do
    GenServer.call(server, :get_status)
  end

  @spec get_endpoint(GenServer.server()) :: String.t() | nil
  def get_endpoint(server) do
    GenServer.call(server, :get_endpoint)
  end

  @spec get_info(GenServer.server()) :: map()
  def get_info(server) do
    GenServer.call(server, :get_info)
  end

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    config = Keyword.get(opts, :config, Config.load())

    state = %{
      id: Keyword.get_lazy(opts, :id, &generate_id/0),
      config: config,
      status: :stopped,
      server_pid: nil,
      port: nil,
      endpoint: nil,
      log_file: nil,
      server_dir: nil,
      error: nil,
      diagnostics: nil,
      crash_monitor: Keyword.get(opts, :crash_monitor)
    }

    {:ok, state}
  end

  @impl true
  def handle_call({:deploy, timeout}, _from, %{status: :stopped} = state) do
    case do_deploy(state, timeout) do
      {:ok, new_state} ->
        {:reply, :ok, new_state}

      {:error, reason, new_state} ->
        {:reply, {:error, reason}, new_state}
    end
  end

  def handle_call({:deploy, _timeout}, _from, state) do
    {:reply, {:error, {:invalid_status, state.status}}, state}
  end

  def handle_call({:shutdown, timeout}, _from, %{status: :ready} = state) do
    new_state = do_shutdown(state, timeout)
    {:reply, :ok, new_state}
  end

  def handle_call({:shutdown, _timeout}, _from, %{status: :stopped} = state) do
    {:reply, :ok, state}
  end

  def handle_call({:shutdown, timeout}, _from, %{status: :failed} = state) do
    new_state = do_cleanup(state, timeout)
    {:reply, :ok, new_state}
  end

  def handle_call({:shutdown, _timeout}, _from, state) do
    {:reply, {:error, {:invalid_status, state.status}}, state}
  end

  def handle_call(:get_status, _from, state) do
    {:reply, state.status, state}
  end

  def handle_call(:get_endpoint, _from, state) do
    {:reply, state.endpoint, state}
  end

  def handle_call(:get_info, _from, state) do
    info = %{
      id: state.id,
      status: state.status,
      endpoint: state.endpoint,
      port: state.port,
      log_file: state.log_file,
      error: state.error,
      diagnostics: state.diagnostics
    }

    {:reply, info, state}
  end

  @impl true
  def handle_info({:server_crashed, server_id, crash_info}, state) do
    Logger.error("[Toast.Controller] Server #{server_id} crashed: #{inspect(crash_info)}")
    notify_crash_monitor(state.crash_monitor, server_id, crash_info)
    {:noreply, %{state | status: :failed, error: {:server_crashed, crash_info}}}
  end

  def handle_info(msg, state) do
    Logger.debug("[Toast.Controller] Unexpected message: #{inspect(msg)}")
    {:noreply, state}
  end

  # --- Deploy sequence ---

  defp do_deploy(state, timeout) do
    Logger.debug("[Toast.Controller] Starting deploy for #{state.id} (timeout=#{timeout}ms)")
    state = %{state | status: :starting}

    with {:ok, port} <- PortAllocator.allocate(),
         _ = Logger.debug("[Toast.Controller] #{state.id}: allocated port #{port}"),
         state = %{state | port: port, endpoint: "http://127.0.0.1:#{port}"},
         {:ok, launch_spec} <- Factory.build_single_server(state.config, state.id, port),
         state = %{state | log_file: launch_spec.log_file, server_dir: launch_spec.server_dir},
         {:ok, server_pid} <- start_server_process(launch_spec),
         _ = Logger.debug("[Toast.Controller] #{state.id}: server process started (#{inspect(server_pid)})"),
         state = %{state | server_pid: server_pid},
         :ok <- ServerProcess.launch(server_pid),
         _ = Logger.debug("[Toast.Controller] #{state.id}: OS process launched, waiting for health check"),
         :ok <- wait_for_ready(state, timeout) do
      Logger.info("[Toast.Controller] Deployment #{state.id} ready at #{state.endpoint}")
      {:ok, %{state | status: :ready}}
    else
      {:error, reason} ->
        Logger.error("[Toast.Controller] Deploy failed for #{state.id}: #{inspect(reason)}")
        failed_state = rollback(state, reason)
        {:error, reason, failed_state}
    end
  end

  defp start_server_process(launch_spec) do
    opts = [
      id: launch_spec.id,
      executable: launch_spec.executable,
      args: launch_spec.args,
      env: launch_spec.env,
      working_dir: launch_spec.working_dir,
      listener: self()
    ]

    Toast.Process.Supervisor.start_server(opts)
  end

  defp wait_for_ready(state, timeout) do
    process_check_fn = fn -> ServerProcess.status(state.server_pid) == :running end

    Health.wait_until_ready(state.endpoint,
      timeout: timeout,
      process_check_fn: process_check_fn
    )
  end

  # --- Shutdown sequence ---

  defp do_shutdown(state, timeout) do
    Logger.debug("[Toast.Controller] Shutting down deployment #{state.id}")
    state = %{state | status: :stopping}
    do_cleanup(state, timeout)
  end

  defp do_cleanup(state, timeout) do
    Logger.debug("[Toast.Controller] Cleaning up #{state.id}")
    stop_server(state, timeout)
    diagnostics = collect_diagnostics(state)
    Logger.debug("[Toast.Controller] #{state.id}: diagnostics collected")
    cleanup_dirs(state)
    %{state | status: :stopped, server_pid: nil, diagnostics: diagnostics}
  end

  defp collect_diagnostics(%{server_dir: nil}), do: nil

  defp collect_diagnostics(state) do
    sanitizer_errors = Sanitizer.collect_errors(state.server_dir, state.id)
    log_content = Toast.Utils.Filesystem.read_file_or_nil(state.log_file)

    %{
      sanitizer_errors: sanitizer_errors,
      server_log: if(log_content, do: ServerLog.scan(log_content)),
      crash_report: if(log_content, do: CrashLogParser.parse(log_content))
    }
  end

  defp stop_server(%{server_pid: nil}, _timeout), do: :ok

  defp stop_server(%{server_pid: pid}, timeout) do
    ServerProcess.stop(pid, timeout)
    DynamicSupervisor.terminate_child(Toast.Process.Supervisor, pid)
  catch
    :exit, _ -> :ok
  end

  defp cleanup_dirs(%{server_dir: nil}), do: :ok

  defp cleanup_dirs(%{server_dir: server_dir}) do
    Toast.Utils.Filesystem.cleanup_server_dirs(server_dir)
  end

  # --- Rollback on deploy failure ---

  defp rollback(state, reason) do
    Logger.debug("[Toast.Controller] Rolling back #{state.id} due to: #{inspect(reason)}")
    stop_server(state, 5_000)
    cleanup_dirs(state)
    %{state | status: :failed, server_pid: nil, error: reason}
  end

  # --- Helpers ---

  defp notify_crash_monitor(nil, _id, _info), do: :ok
  defp notify_crash_monitor(pid, id, info), do: send(pid, {:server_crashed, id, info})

  defp generate_id do
    "toast-#{System.unique_integer([:positive])}"
  end
end
