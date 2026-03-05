defmodule Toast.Deployment.Controller do
  @moduledoc "GenServer orchestrating the lifecycle of an ArangoDB deployment."

  use GenServer

  require Logger

  alias Toast.Config
  alias Toast.Deployment.{ServerInstance, ServerLifecycle}

  @type status :: :stopped | :starting | :ready | :degraded | :stopping | :failed

  # --- Behaviour callbacks for mode-specific logic ---

  @callback init_mode_state() :: map()
  @callback init_servers(String.t()) :: %{String.t() => ServerInstance.t()}
  @callback deploy(State.t(), timeout()) :: {:ok, State.t()} | {:error, term(), State.t()}
  @callback shutdown(State.t(), timeout()) :: State.t()
  @callback derive_status(%{String.t() => ServerInstance.t()}) :: atom()
  @callback resolve_target(State.t(), term()) :: {:ok, [String.t()]} | {:error, term()}
  @callback build_info(State.t()) :: map()

  @callback handle_call_extra(term(), GenServer.from(), State.t()) ::
              {:reply, term(), State.t()} | {:noreply, State.t()} | :not_handled
  @optional_callbacks [handle_call_extra: 3]

  defmodule State do
    @moduledoc false
    @enforce_keys [:config, :mode]
    defstruct [
      :config,
      :id,
      :error,
      :diagnostics,
      :on_crash,
      :on_event,
      mode: nil,
      mode_state: %{},
      status: :stopped,
      servers: %{},
      expected_crashes: %{}
    ]
  end

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
  def deploy(server, timeout \\ 120_000) do
    GenServer.call(server, {:deploy, timeout}, timeout + 5_000)
  end

  @spec shutdown(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def shutdown(server, timeout \\ 60_000) do
    GenServer.call(server, {:shutdown, timeout}, timeout + 5_000)
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
    do: GenServer.call(server, {:restart_server, server_id, opts}, 65_000)

  @spec start_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def start_server(server, server_id, opts \\ []),
    do: GenServer.call(server, {:start_server, server_id, opts}, 65_000)

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
      when status in [:ready, :degraded] do
    new_state = state.mode.shutdown(state, timeout)
    {:reply, :ok, new_state}
  end

  def handle_call({:shutdown, _timeout}, _from, %{status: :stopped} = state) do
    {:reply, :ok, state}
  end

  def handle_call({:shutdown, timeout}, _from, %{status: :failed} = state) do
    new_state = state.mode.shutdown(state, timeout)
    {:reply, :ok, new_state}
  end

  def handle_call({:shutdown, _timeout}, _from, state) do
    {:reply, {:error, {:invalid_status, state.status}}, state}
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

  def handle_call(msg, from, state) do
    if function_exported?(state.mode, :handle_call_extra, 3) do
      case state.mode.handle_call_extra(msg, from, state) do
        :not_handled ->
          Logger.debug("Unhandled call: #{inspect(msg)}")
          {:reply, {:error, :not_handled}, state}

        result ->
          result
      end
    else
      Logger.debug("Unhandled call: #{inspect(msg)}")
      {:reply, {:error, :not_handled}, state}
    end
  end

  @impl true
  def handle_info({:server_crashed, server_id, crash_info}, state) do
    server = state.servers[server_id]

    on_crash_ctx = %{
      on_crash: state.on_crash,
      on_event: state.on_event,
      deployment: build_deployment_from_state(state)
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
        state = update_server(state, server_id, operational_state: :crashed, expecting_exit: true)
        state = %{state | status: state.mode.derive_status(state.servers)}
        {:noreply, state}

      :intentional_exit ->
        {:noreply, state}

      :crash_during_intentional_stop ->
        stop_health_monitor(state, server_id)
        state = update_server(state, server_id, operational_state: :crashed, expecting_exit: false)
        {:noreply, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}

      :unexpected_crash ->
        state =
          if server, do: update_server(state, server_id, operational_state: :crashed), else: state

        stop_health_monitor(state, server_id)
        {:noreply, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}
    end
  end

  def handle_info({:server_unhealthy, server_id}, state) do
    Logger.error("Server #{server_id} is unresponsive, killing process")
    stop_server_process(state, server_id, 5_000 * state.config.timeout_factor)
    crash_info = %{exit_status: nil, signal: nil, timestamp: DateTime.utc_now()}
    ServerLifecycle.notify_crash(state.on_crash, build_deployment_from_state(state), crash_info)

    ServerLifecycle.notify_event(
      state.on_event,
      {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()}
    )

    {:noreply, %{state | status: :failed, error: {:server_unhealthy, server_id}}}
  end

  def handle_info({:expect_crash_timeout, server_id}, state) do
    server = Map.get(state.servers, server_id)

    expected_crashes =
      ServerLifecycle.handle_expect_crash_timeout(server_id, state.expected_crashes, server)

    {:noreply, %{state | expected_crashes: expected_crashes}}
  end

  def handle_info({:verify_crash_check, server_id, from, deadline}, state) do
    server = Map.get(state.servers, server_id)

    {_tag, expected_crashes} =
      ServerLifecycle.handle_verify_crash_check(
        server_id,
        from,
        deadline,
        state.expected_crashes,
        server
      )

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

        case start_single_health_monitor(server_id, server.endpoint) do
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

  # --- Control operations ---

  defp do_stop_server(server_id, acc) do
    with {:ok, server} <- fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, :running) do
      ServerLifecycle.stop_server(server, timeout_factor: acc.config.timeout_factor)
      acc = update_server(acc, server_id, operational_state: :stopped, expecting_exit: true)

      ServerLifecycle.notify_event(
        acc.on_event,
        {:server_stopped, server_id, server.pid, nil, DateTime.utc_now()}
      )

      {:ok, acc}
    end
  end

  defp do_kill_server(server_id, acc) do
    with {:ok, server} <- fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, :running) do
      ServerLifecycle.kill_server(server)
      acc = update_server(acc, server_id, operational_state: :killed, expecting_exit: true)
      ServerLifecycle.notify_event(acc.on_event, {:server_killed, server_id, DateTime.utc_now()})
      {:ok, acc}
    end
  end

  defp do_pause_server(server_id, acc) do
    with {:ok, server} <- fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, :running) do
      ServerLifecycle.pause_server(server)
      acc = update_server(acc, server_id, operational_state: :paused, expecting_exit: true)
      ServerLifecycle.notify_event(acc.on_event, {:server_paused, server_id, DateTime.utc_now()})
      {:ok, acc}
    end
  end

  defp do_resume_server(server_id, acc) do
    with {:ok, server} <- fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, :paused) do
      ServerLifecycle.resume_server(server)
      acc = update_server(acc, server_id, operational_state: :running, expecting_exit: false)
      ServerLifecycle.notify_event(acc.on_event, {:server_resumed, server_id, DateTime.utc_now()})
      {:ok, acc}
    end
  end

  defp do_restart_server(server_id, acc, opts) do
    with {:ok, server} <- fetch_server(acc, server_id) do
      ServerLifecycle.stop_before_restart(server, timeout_factor: acc.config.timeout_factor)
      acc = update_server(acc, server_id, operational_state: :stopped, expecting_exit: true)
      relaunch_server(server_id, acc, server, opts)
    end
  end

  defp do_start_server(server_id, acc, opts) do
    with {:ok, server} <- fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state_in(server, [:stopped, :killed, :crashed]) do
      relaunch_server(server_id, acc, server, opts)
    end
  end

  defp relaunch_server(server_id, acc, server, opts) do
    opts = Keyword.put_new(opts, :timeout_factor, acc.config.timeout_factor)

    case ServerLifecycle.relaunch_and_wait(server, opts) do
      :ok ->
        acc = update_server(acc, server_id, operational_state: :running, expecting_exit: false)
        {:ok, acc}

      {:error, _} = err ->
        err
    end
  end

  defp do_expect_crash(server_id, acc, timeout) do
    with {:ok, server} <- fetch_server(acc, server_id) do
      case ServerLifecycle.expect_crash(server_id, timeout, acc.expected_crashes, server) do
        {:ok, expected_crashes} ->
          {:ok, %{acc | expected_crashes: expected_crashes}}

        {:error, _} = err ->
          err
      end
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
    Enum.reduce_while(server_ids, {:ok, state}, fn server_id, {:ok, acc} ->
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

  # --- Server/state helpers (public for mode modules) ---

  @doc false
  def fetch_server(state, server_id) do
    case Map.get(state.servers, server_id) do
      nil -> {:error, :not_found}
      server -> {:ok, server}
    end
  end

  @doc false
  def update_server(state, server_id, updates) do
    server = state.servers[server_id]
    updated = struct!(server, updates)
    %{state | servers: Map.put(state.servers, server_id, updated)}
  end

  # --- Health monitoring ---

  @doc false
  def start_single_health_monitor(server_id, endpoint) do
    case Toast.Process.Supervisor.start_health_monitor(
           server_id: server_id,
           endpoint: endpoint,
           listener: self()
         ) do
      {:ok, pid} ->
        Process.monitor(pid)
        {:ok, pid}

      error ->
        error
    end
  end

  @doc false
  def stop_all_health_monitors(state) do
    for {_id, server} <- state.servers do
      ServerLifecycle.stop_health_monitor(server)
    end
  end

  @doc false
  def stop_health_monitor(state, server_id) do
    case state.servers[server_id] do
      nil -> :ok
      server -> ServerLifecycle.stop_health_monitor(server)
    end
  end

  @doc false
  def stop_server_process(state, server_id, timeout) do
    case state.servers[server_id] do
      %{server_pid: nil} ->
        :ok

      %{server_pid: pid} ->
        try do
          Toast.Process.ServerProcess.stop(pid, timeout)
          DynamicSupervisor.terminate_child(Toast.Process.Supervisor, pid)
        catch
          :exit, _ -> :ok
        end

      nil ->
        :ok
    end
  end

  defp find_server_by_health_monitor(state, pid) do
    Enum.find(state.servers, fn {_id, server} -> server.health_monitor == pid end)
  end

  # --- Shared helpers for mode modules ---

  @doc false
  def spec_to_server_opts(spec) do
    [
      id: spec.id,
      executable: spec.executable,
      args: spec.args,
      env: spec.env,
      working_dir: spec.working_dir,
      listener: self(),
      output_handler: &ServerLifecycle.print_server_output/2
    ]
  end

  @doc false
  def collect_diagnostics(state, error_for_server_fn) do
    Map.new(state.servers, fn {server_id, server} ->
      {server_id, Toast.Diagnostics.build_server_diagnostics(server, error_for_server_fn.(server_id))}
    end)
  end

  @doc false
  def finalize_shutdown(state, diagnostics) do
    %{
      state
      | status: :stopped,
        servers: clear_server_pids(state.servers),
        diagnostics: diagnostics
    }
  end

  @doc false
  def clear_server_pids(servers) do
    Map.new(servers, fn {id, server} -> {id, %{server | server_pid: nil, health_monitor: nil}} end)
  end

  @doc false
  def build_deployment_from_state(state) do
    primary_endpoint =
      state.mode.build_info(state)
      |> Map.get(:primary_endpoint, "")

    %Toast.Deployment{
      id: state.id,
      mode: deployment_mode(state.mode),
      config: state.config,
      controller: self(),
      endpoint: primary_endpoint,
      work_dir: state.config.work_dir
    }
  end

  defp deployment_mode(Toast.Deployment.Controller.SingleServer), do: :single_server
  defp deployment_mode(Toast.Deployment.Controller.Cluster), do: :cluster

  @doc false
  def remaining_ms(deadline) do
    max(0, deadline - System.monotonic_time(:millisecond))
  end

  defp generate_id(Toast.Deployment.Controller.SingleServer) do
    "toast-#{System.unique_integer([:positive])}"
  end

  defp generate_id(Toast.Deployment.Controller.Cluster) do
    "toast-cluster-#{System.unique_integer([:positive])}"
  end
end
