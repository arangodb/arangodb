defmodule Toast.Deployment.Controller do
  @moduledoc "GenServer orchestrating the lifecycle of an ArangoDB deployment."

  use GenServer

  require Logger

  alias Toast.Config
  alias Toast.Deployment.Controller.Helpers
  alias Toast.Deployment.{ServerInstance, ServerLifecycle}
  alias Toast.Process.CrashEvent

  @type status :: :stopped | :starting | :ready | :degraded | :stopping | :failed

  @type deployment_error ::
          {:server_crashed, String.t(), Toast.Process.CrashInfo.t()}
          | {:server_unhealthy, String.t()}
          | nil

  # --- Behaviour callbacks for mode-specific logic ---

  @callback init_mode_state() :: map()
  @callback init_servers(String.t()) :: %{String.t() => ServerInstance.t()}
  @callback deploy(__MODULE__.State.t(), timeout()) ::
              {:ok, __MODULE__.State.t()} | {:error, term(), __MODULE__.State.t()}
  @callback shutdown(__MODULE__.State.t(), timeout()) :: __MODULE__.State.t()
  @callback derive_status(%{String.t() => ServerInstance.t()}) :: atom()
  @callback resolve_target(__MODULE__.State.t(), term()) ::
              {:ok, [String.t()]} | {:error, term()}
  @callback build_info(__MODULE__.State.t()) :: map()

  @callback handle_call_extra(term(), GenServer.from(), __MODULE__.State.t()) ::
              {:reply, term(), __MODULE__.State.t()}
              | {:noreply, __MODULE__.State.t()}
              | :not_handled
  @optional_callbacks [handle_call_extra: 3]

  defmodule State do
    @moduledoc false
    @enforce_keys [:config, :mode]
    defstruct [
      :config,
      :id,
      :error,
      :on_crash,
      :on_event,
      mode: nil,
      mode_state: %{},
      status: :stopped,
      servers: %{},
      expected_crashes: %{}
    ]

    @type t :: %__MODULE__{
            config: Toast.Config.t(),
            id: String.t() | nil,
            error: Toast.Deployment.Controller.deployment_error(),
            on_crash: (term(), term() -> term()) | nil,
            on_event: (term() -> term()) | nil,
            mode: module() | nil,
            mode_state: map(),
            status: Toast.Deployment.Controller.status(),
            servers: %{optional(String.t()) => ServerInstance.t()},
            expected_crashes: map()
          }
  end

  # --- Client API ---

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts) do
    {name, init_opts} = Keyword.pop(opts, :name)
    server_opts = if name, do: [name: name], else: []
    GenServer.start_link(__MODULE__, init_opts, server_opts)
  end

  @spec deploy(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def deploy(server, timeout \\ 120_000) do
    GenServer.call(server, {:deploy, timeout}, timeout + 5_000)
  end

  @spec shutdown(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def shutdown(server, timeout \\ 60_000) do
    GenServer.call(server, {:shutdown, timeout}, timeout + 5_000)
  end

  @doc """
  Abort all running servers by sending SIGABRT.

  Registers each server as expecting a crash (so crashes are classified as
  expected) and sends SIGABRT to trigger the crash handler (backtrace + coredump).
  Returns a list of maps describing each aborted server.
  """
  @spec abort(GenServer.server()) :: [map()]
  def abort(server) do
    GenServer.call(server, :abort, 10_000)
  catch
    :exit, _ -> []
  end

  @spec dump_agency(GenServer.server(), timeout()) :: term()
  def dump_agency(server, timeout \\ 60_000) do
    GenServer.call(server, :dump_agency, timeout)
  end

  @spec get_status(GenServer.server()) :: status()
  def get_status(server) do
    GenServer.call(server, :get_status)
  end

  @spec get_info(GenServer.server()) :: map()
  def get_info(server) do
    GenServer.call(server, :get_info)
  end

  @spec stop_server(GenServer.server(), term()) :: :ok | {:error, term()}
  def stop_server(server, server_id), do: GenServer.call(server, {:stop_server, server_id})
  @spec kill_server(GenServer.server(), term()) :: :ok | {:error, term()}
  def kill_server(server, server_id), do: GenServer.call(server, {:kill_server, server_id})
  @spec pause_server(GenServer.server(), term()) :: :ok | {:error, term()}
  def pause_server(server, server_id), do: GenServer.call(server, {:pause_server, server_id})
  @spec resume_server(GenServer.server(), term()) :: :ok | {:error, term()}
  def resume_server(server, server_id), do: GenServer.call(server, {:resume_server, server_id})

  @spec restart_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def restart_server(server, server_id, opts \\ []),
    do: GenServer.call(server, {:restart_server, server_id, opts}, :infinity)

  @spec start_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def start_server(server, server_id, opts \\ []),
    do: GenServer.call(server, {:start_server, server_id, opts}, :infinity)

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    config = Keyword.get(opts, :config, Config.load())
    mode = Keyword.fetch!(opts, :mode)
    id = Keyword.get_lazy(opts, :id, fn -> generate_id(mode) end)

    state = %State{
      id: id,
      config: config,
      mode: mode,
      mode_state: mode.init_mode_state(),
      servers: mode.init_servers(id),
      on_crash: Keyword.get(opts, :on_crash),
      on_event: Keyword.get(opts, :on_event)
    }

    {:ok, state}
  end

  @impl true
  def handle_call({:deploy, timeout}, _from, %{status: :stopped} = state) do
    case state.mode.deploy(state, timeout) do
      {:ok, new_state} ->
        {:reply, :ok, new_state}

      {:error, reason, new_state} ->
        {:reply, {:error, reason}, new_state}
    end
  end

  def handle_call({:deploy, _timeout}, _from, state) do
    {:reply, {:error, {:invalid_status, state.status}}, state}
  end

  def handle_call({:shutdown, timeout}, _from, %{status: status} = state)
      when status in [:ready, :degraded, :failed] do
    new_state = state.mode.shutdown(state, timeout)
    {:reply, :ok, new_state}
  end

  def handle_call({:shutdown, _timeout}, _from, %{status: :stopped} = state) do
    {:reply, :ok, state}
  end

  def handle_call({:shutdown, _timeout}, _from, state) do
    {:reply, {:error, {:invalid_status, state.status}}, state}
  end

  def handle_call(:abort, _from, state) do
    {killed_servers, new_state} = do_abort_all_servers(state)
    {:reply, killed_servers, new_state}
  end

  def handle_call(:get_status, _from, state) do
    {:reply, state.status, state}
  end

  def handle_call(:get_info, _from, state) do
    info = state.mode.build_info(state)
    {:reply, info, state}
  end

  def handle_call({:get_server, server_id}, _from, state) do
    case Map.get(state.servers, server_id) do
      nil -> {:reply, {:error, :not_found}, state}
      server -> {:reply, server, state}
    end
  end

  def handle_call(:get_servers, _from, state) do
    {:reply, Map.values(state.servers), state}
  end

  def handle_call({:get_servers, role}, _from, state) do
    servers =
      state.servers
      |> Map.values()
      |> Enum.filter(&(&1.role == role))

    {:reply, servers, state}
  end

  def handle_call({:stop_server, target}, _from, state) do
    resolve_and_apply(state, target, &do_stop_server/2)
  end

  def handle_call({:kill_server, target}, _from, state) do
    resolve_and_apply(state, target, &do_kill_server/2)
  end

  def handle_call({:pause_server, target}, _from, state) do
    resolve_and_apply(state, target, &do_pause_server/2)
  end

  def handle_call({:resume_server, target}, _from, state) do
    resolve_and_apply(state, target, &do_resume_server/2)
  end

  def handle_call({:restart_server, target, opts}, _from, state) do
    resolve_and_apply(state, target, &do_restart_server(&1, &2, opts))
  end

  def handle_call({:start_server, target, opts}, _from, state) do
    resolve_and_apply(state, target, &do_start_server(&1, &2, opts))
  end

  def handle_call({:expect_crash, target, timeout}, _from, state) do
    resolve_and_apply(state, target, &do_expect_crash(&1, &2, timeout))
  end

  def handle_call({:verify_crash, server_id, timeout}, from, state) do
    case ServerLifecycle.verify_crash(server_id, timeout, state.expected_crashes, from) do
      {:reply, result, expected_crashes} ->
        {:reply, result, %{state | expected_crashes: expected_crashes}}

      {:noreply, expected_crashes} ->
        {:noreply, %{state | expected_crashes: expected_crashes}}
    end
  end

  def handle_call({:resolve_target, target}, _from, state) do
    {:reply, state.mode.resolve_target(state, target), state}
  end

  def handle_call(msg, from, state) do
    result =
      if function_exported?(state.mode, :handle_call_extra, 3),
        do: state.mode.handle_call_extra(msg, from, state),
        else: :not_handled

    case result do
      :not_handled ->
        Logger.debug("Unhandled call: #{inspect(msg)}")
        {:reply, {:error, :not_handled}, state}

      reply ->
        reply
    end
  end

  @impl true
  def handle_info({:server_crashed, server_id, crash_info}, state) do
    server = state.servers[server_id]

    on_crash_ctx = %{
      on_crash: state.on_crash,
      on_event: state.on_event
    }

    case ServerLifecycle.handle_crash(
           server_id,
           crash_info,
           state.expected_crashes,
           server,
           on_crash_ctx
         ) do
      {:expected, expected_crashes} ->
        state = %{state | expected_crashes: expected_crashes}

        state =
          Helpers.update_server(state, server_id,
            operational_state: :crashed,
            expecting_exit: true
          )

        state = %{state | status: state.mode.derive_status(state.servers)}
        {:noreply, state}

      :intentional_exit ->
        {:noreply, state}

      :crash_during_intentional_stop ->
        Helpers.stop_health_monitor(state, server_id)

        state =
          Helpers.update_server(state, server_id,
            operational_state: :crashed,
            expecting_exit: false
          )

        {:noreply, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}

      :unexpected_crash ->
        state =
          if server,
            do: Helpers.update_server(state, server_id, operational_state: :crashed),
            else: state

        Helpers.stop_health_monitor(state, server_id)
        {:noreply, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}
    end
  end

  def handle_info({:server_unhealthy, server_id}, state) do
    Logger.error("Server #{server_id} is unresponsive, killing process")
    Helpers.stop_server_process(state, server_id, 5_000 * state.config.timeout_factor)

    state =
      Helpers.update_server(state, server_id, operational_state: :killed, expecting_exit: true)

    crash_info = %Toast.Process.CrashInfo{
      exit_status: nil,
      signal: nil,
      timestamp: DateTime.utc_now()
    }

    ServerLifecycle.notify_crash(state.on_crash, server_id, crash_info)

    ServerLifecycle.notify_event(
      state.on_event,
      {:server_crashed, %CrashEvent{server_id: server_id, crash_info: crash_info}}
    )

    {:noreply, %{state | status: :failed, error: {:server_unhealthy, server_id}}}
  end

  def handle_info({:expect_crash_timeout, server_id}, state) do
    server = Map.get(state.servers, server_id)

    expected_crashes =
      ServerLifecycle.handle_expect_crash_timeout(server_id, state.expected_crashes, server)

    {:noreply, %{state | expected_crashes: expected_crashes}}
  end

  def handle_info({:verify_crash_timeout, server_id}, state) do
    server = Map.get(state.servers, server_id)

    expected_crashes =
      ServerLifecycle.handle_verify_crash_timeout(server_id, state.expected_crashes, server)

    {:noreply, %{state | expected_crashes: expected_crashes}}
  end

  def handle_info({:DOWN, _ref, :process, pid, reason}, state)
      when reason not in [:normal, :shutdown] do
    case find_server_by_health_monitor(state, pid) do
      {server_id, _server} when state.status in [:ready, :degraded] ->
        Logger.warning(
          "HealthMonitor for #{server_id} died unexpectedly (#{inspect(reason)}), restarting"
        )

        server = state.servers[server_id]

        case Helpers.start_single_health_monitor(server_id, server.endpoint) do
          {:ok, new_pid} ->
            updated = %{server | health_monitor: new_pid}
            {:noreply, %{state | servers: Map.put(state.servers, server_id, updated)}}

          {:error, _} ->
            {:noreply, state}
        end

      _ ->
        {:noreply, state}
    end
  end

  def handle_info(msg, state) do
    Logger.debug("Unexpected message: #{inspect(msg)}")
    {:noreply, state}
  end

  # --- Abort all servers ---

  @abort_timeout 60_000

  defp do_abort_all_servers(state) do
    running_servers =
      Enum.filter(state.servers, fn {_id, server} ->
        server.operational_state in [:running, :paused]
      end)

    # Register expected crashes and send SIGABRT to each server
    expected_crashes =
      Enum.reduce(running_servers, state.expected_crashes, fn {server_id, server}, acc ->
        case ServerLifecycle.expect_crash(server_id, @abort_timeout, acc, server) do
          {:ok, updated} -> updated
          {:error, :already_expected} -> acc
        end
      end)

    # Collect info before sending signals (process may die quickly)
    killed_servers =
      Enum.map(running_servers, fn {server_id, server} ->
        os_pid = Toast.Process.ServerProcess.os_pid(server.server_pid)
        ServerLifecycle.abort_server(server)

        %{server_id: server_id, os_pid: os_pid, log_file: server.log_file}
      end)

    {killed_servers, %{state | expected_crashes: expected_crashes}}
  end

  # --- Control operations ---

  defp do_stop_server(server_id, acc) do
    with {:ok, server} <- Helpers.fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, :running) do
      ServerLifecycle.stop_server(server, timeout_factor: acc.config.timeout_factor)

      acc =
        Helpers.update_server(acc, server_id, operational_state: :stopped, expecting_exit: true)

      ServerLifecycle.notify_event(
        acc.on_event,
        {:server_stopped, server_id, server.pid, nil, DateTime.utc_now()}
      )

      {:ok, acc}
    end
  end

  defp do_kill_server(server_id, acc) do
    with {:ok, server} <- Helpers.fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, :running) do
      ServerLifecycle.kill_server(server)

      acc =
        Helpers.update_server(acc, server_id, operational_state: :killed, expecting_exit: true)

      ServerLifecycle.notify_event(acc.on_event, {:server_killed, server_id, DateTime.utc_now()})
      {:ok, acc}
    end
  end

  defp do_pause_server(server_id, acc) do
    with {:ok, server} <- Helpers.fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, :running) do
      ServerLifecycle.pause_server(server)

      acc =
        Helpers.update_server(acc, server_id, operational_state: :paused, expecting_exit: true)

      ServerLifecycle.notify_event(acc.on_event, {:server_paused, server_id, DateTime.utc_now()})
      {:ok, acc}
    end
  end

  defp do_resume_server(server_id, acc) do
    with {:ok, server} <- Helpers.fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, :paused) do
      ServerLifecycle.resume_server(server)

      acc =
        Helpers.update_server(acc, server_id, operational_state: :running, expecting_exit: false)

      ServerLifecycle.notify_event(acc.on_event, {:server_resumed, server_id, DateTime.utc_now()})
      {:ok, acc}
    end
  end

  defp do_restart_server(server_id, acc, opts) do
    with {:ok, server} <- Helpers.fetch_server(acc, server_id) do
      ServerLifecycle.stop_before_restart(server, timeout_factor: acc.config.timeout_factor)

      acc =
        Helpers.update_server(acc, server_id, operational_state: :stopped, expecting_exit: true)

      relaunch_server(server_id, acc, server, opts)
    end
  end

  defp do_start_server(server_id, acc, opts) do
    with {:ok, server} <- Helpers.fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state_in(server, [:stopped, :killed, :crashed]) do
      relaunch_server(server_id, acc, server, opts)
    end
  end

  defp relaunch_server(server_id, acc, server, opts) do
    opts = Keyword.put_new(opts, :timeout_factor, acc.config.timeout_factor)

    with :ok <- ServerLifecycle.relaunch_and_wait(server, opts) do
      acc =
        Helpers.update_server(acc, server_id,
          operational_state: :running,
          expecting_exit: false
        )

      {:ok, acc}
    end
  end

  defp do_expect_crash(server_id, acc, timeout) do
    with {:ok, server} <- Helpers.fetch_server(acc, server_id),
         {:ok, expected_crashes} <-
           ServerLifecycle.expect_crash(server_id, timeout, acc.expected_crashes, server) do
      {:ok, %{acc | expected_crashes: expected_crashes}}
    end
  end

  # --- Control helpers ---

  defp resolve_and_apply(state, target, fun) do
    case state.mode.resolve_target(state, target) do
      {:ok, server_ids} -> apply_to_each(server_ids, state, fun)
      {:error, _} = err -> {:reply, err, state}
    end
  end

  defp apply_to_each(server_ids, state, fun) do
    server_ids
    |> Enum.reduce_while({:ok, state}, fn server_id, {:ok, acc} ->
      case fun.(server_id, acc) do
        {:ok, new_acc} -> {:cont, {:ok, new_acc}}
        {:error, _} = err -> {:halt, err}
      end
    end)
    |> case do
      {:ok, final_state} ->
        final_state = %{final_state | status: final_state.mode.derive_status(final_state.servers)}
        {:reply, :ok, final_state}

      {:error, _} = err ->
        {:reply, err, state}
    end
  end

  defp find_server_by_health_monitor(state, pid) do
    Enum.find(state.servers, fn {_id, server} -> server.health_monitor == pid end)
  end

  defp generate_id(Toast.Deployment.Controller.SingleServer) do
    "toast-#{System.unique_integer([:positive])}"
  end

  defp generate_id(Toast.Deployment.Controller.Cluster) do
    "toast-cluster-#{System.unique_integer([:positive])}"
  end
end
