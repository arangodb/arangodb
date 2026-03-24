defmodule Toast.Deployment.Controller do
  @moduledoc "GenServer orchestrating the lifecycle of an ArangoDB deployment."

  use GenServer

  require Logger

  alias Toast.Config
  alias Toast.Deployment.{Health, ServerInstance, ServerLifecycle}
  alias Toast.Diagnostics.AgencyDump
  alias Toast.Process.ServerProcess
  alias Toast.Process.Supervisor, as: ProcessSupervisor
  alias ToastTest.EventStore

  @type status :: :stopped | :starting | :ready | :degraded | :stopping | :failed

  @type deployment_error ::
          {:server_crashed, String.t(), Toast.Process.CrashInfo.t()}
          | {:server_unhealthy, String.t()}
          | nil

  # Buffer for Task.async_stream to account for scheduling/collection overhead
  # beyond the per-task timeout.
  @task_stream_buffer 5_000

  # Deploy order: single and agent are both first (they never coexist in one deployment)
  @role_deploy_order [:single, :agent, :dbserver, :coordinator]

  defmodule State do
    @moduledoc false
    @enforce_keys [:config]
    defstruct [
      :config,
      :id,
      :error,
      status: :stopped,
      servers: %{},
      expected_crashes: %{},
      agency_dump: nil
    ]

    @type t :: %__MODULE__{
            config: Toast.Config.t(),
            id: String.t() | nil,
            error: Toast.Deployment.Controller.deployment_error(),
            status: Toast.Deployment.Controller.status(),
            servers: %{optional(String.t()) => Toast.Deployment.ServerInstance.t()},
            expected_crashes: map(),
            agency_dump: term()
          }
  end

  # --- Client API ---

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts) do
    {name, init_opts} = Keyword.pop(opts, :name)
    server_opts = if name, do: [name: name], else: []
    GenServer.start_link(__MODULE__, init_opts, server_opts)
  end

  @spec deploy(GenServer.server(), [map()], timeout(), keyword()) :: :ok | {:error, term()}
  def deploy(server, specs, timeout \\ 120_000, opts \\ []) do
    # Use :infinity because on failure the error cleanup (abort_all_servers +
    # rollback) can take significantly longer than the deploy timeout itself.
    # Internal timeouts (health check deadlines, abort waits) are all bounded.
    GenServer.call(server, {:deploy, specs, timeout, opts}, :infinity)
  end

  @spec shutdown(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def shutdown(server, timeout \\ 60_000) do
    # Use :infinity because the actual shutdown time depends on deployment-specific
    # factors (number of sequential phases, escalation cascades) that the caller
    # can't predict. The controller has comprehensive internal timeout management:
    # per-phase deadlines, Task.async_stream timeouts, and ServerProcess
    # escalation timers — all bounded. Deployment.stop/2 catches :exit if the
    # controller process dies.
    GenServer.call(server, {:shutdown, timeout}, :infinity)
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

  @spec stop_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def stop_server(server, server_id, _opts \\ []),
    do: GenServer.call(server, {:stop_server, server_id}, :infinity)

  @spec kill_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def kill_server(server, server_id, _opts \\ []),
    do: GenServer.call(server, {:kill_server, server_id}, :infinity)

  @spec pause_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def pause_server(server, server_id, _opts \\ []),
    do: GenServer.call(server, {:pause_server, server_id}, :infinity)

  @spec resume_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def resume_server(server, server_id, _opts \\ []),
    do: GenServer.call(server, {:resume_server, server_id}, :infinity)

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
    id = Keyword.fetch!(opts, :id)

    state = %State{
      id: id,
      config: config
    }

    {:ok, state}
  end

  @impl true
  def handle_call({:deploy, specs, timeout, opts}, _from, %{status: :stopped} = state) do
    case do_deploy(state, specs, timeout, opts) do
      {:ok, new_state} ->
        Logger.debug("Deploy succeeded, status=#{new_state.status}")
        {:reply, :ok, new_state}

      {:error, reason, new_state} ->
        Logger.debug("Deploy failed: #{inspect(reason)}")
        {:reply, {:error, reason}, new_state}
    end
  end

  def handle_call({:shutdown, timeout}, _from, state) do
    Logger.info("Shutting down deployment (status=#{state.status}, timeout=#{timeout}ms)")
    new_state = do_shutdown(state, timeout)
    Logger.info("Shutdown complete, status=#{new_state.status}")
    {:reply, :ok, new_state}
  end

  def handle_call(:abort, _from, state) do
    Logger.info("Aborting all servers")
    {killed_servers, new_state} = do_abort_all_servers(state)
    Logger.info("Aborted #{length(killed_servers)} servers")
    {:reply, killed_servers, new_state}
  end

  def handle_call(:get_status, _from, state) do
    {:reply, state.status, state}
  end

  def handle_call(:get_info, _from, state) do
    {:reply, do_build_info(state), state}
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
    {:reply, resolve_target(state, target), state}
  end

  def handle_call(:dump_agency, _from, state) do
    agents = get_living_agents(state)
    dump = AgencyDump.capture(agents: agents)
    {:reply, dump, %{state | agency_dump: dump}}
  end

  @impl true
  def handle_info({:server_crashed, server_id, crash_info}, state) do
    %ServerInstance{} = server = state.servers[server_id]

    crash_ctx = %{deployment_id: state.id}

    case ServerLifecycle.handle_crash(
           server_id,
           crash_info,
           state.expected_crashes,
           server,
           crash_ctx
         ) do
      {:expected, expected_crashes} ->
        state = %{state | expected_crashes: expected_crashes}

        state =
          update_server(state, server_id,
            operational_state: :crashed,
            expecting_exit: true
          )

        state = %{state | status: derive_status(state.servers)}
        {:noreply, state}

      :intentional_exit ->
        {:noreply, state}

      :crash_during_intentional_stop ->
        stop_health_monitor(state, server_id)

        state =
          update_server(state, server_id,
            operational_state: :crashed,
            expecting_exit: false,
            health_monitor: nil
          )

        {:noreply, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}

      :unexpected_crash ->
        Logger.debug(
          "Deployment status: #{state.status} -> :failed (server #{server_id} crashed)"
        )

        stop_health_monitor(state, server_id)

        state = update_server(state, server_id, operational_state: :crashed, health_monitor: nil)

        {:noreply, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}
    end
  end

  def handle_info({:server_unhealthy, server_id}, state) do
    Logger.error("Server #{server_id} is unresponsive, sending SIGABRT for crash backtrace")
    server = state.servers[server_id]

    crash_ctx = %{deployment_id: state.id}
    ServerLifecycle.handle_unhealthy_server(server_id, server, crash_ctx)

    state = update_server(state, server_id, operational_state: :killed, expecting_exit: true)
    Logger.debug("Deployment status: #{state.status} -> :failed")
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
      {server_id, server}
      when state.status in [:ready, :degraded] and
             server.operational_state == :running ->
        Logger.warning(
          "HealthMonitor for #{server_id} died unexpectedly (#{inspect(reason)}), restarting"
        )

        case start_single_health_monitor(server_id, server.endpoint) do
          {:ok, new_pid} ->
            Logger.info("HealthMonitor for #{server_id} restarted successfully")
            updated = %{server | health_monitor: new_pid}
            {:noreply, %{state | servers: Map.put(state.servers, server_id, updated)}}

          {:error, _} ->
            Logger.warning("Failed to restart HealthMonitor for #{server_id}")
            {:noreply, state}
        end

      _ ->
        {:noreply, state}
    end
  end

  def handle_info({:DOWN, _ref, :process, _pid, reason}, state)
      when reason in [:normal, :shutdown] do
    {:noreply, state}
  end

  def handle_info(msg, state) do
    Logger.debug("Unexpected message: #{inspect(msg)}")
    {:noreply, state}
  end

  # --- Deploy pipeline ---

  defp do_deploy(state, specs, timeout, opts) do
    Logger.debug("Starting deploy for #{state.id} (timeout=#{timeout}ms)")
    deadline = System.monotonic_time(:millisecond) + timeout
    state = %{state | status: :starting}
    state = init_servers_from_specs(state, specs)

    notify_event(state, :deployment_starting, %{
      mode: derive_mode(state),
      stacktrace: opts[:stacktrace],
      specs:
        Enum.map(specs, fn spec ->
          %{id: spec.id, role: spec.role, port: spec.port, log_file: spec.log_file}
        end)
    })

    with {:ok, state} <- start_all_server_processes(state, specs),
         {:ok, state} <- deploy_role_groups(state, specs, deadline),
         {:ok, state} <- start_all_health_monitors(state) do
      state = post_deploy(state)
      servers = Map.new(state.servers, fn {id, s} -> {id, %{s | operational_state: :running}} end)
      state = %{state | status: :ready, servers: servers}

      notify_event(state, :deployment_started, %{
        servers:
          Map.new(state.servers, fn {id, s} ->
            {id, %{role: s.role, endpoint: s.endpoint, log_file: s.log_file}}
          end)
      })

      Logger.info("Deployment #{state.id} ready")
      {:ok, state}
    else
      {:error, reason} ->
        handle_deploy_failure(state, reason, &rollback/2)
    end
  end

  defp init_servers_from_specs(state, specs) do
    servers =
      Map.new(specs, fn spec ->
        {spec.id,
         %ServerInstance{
           id: spec.id,
           role: spec.role,
           port: spec.port,
           endpoint: "http://127.0.0.1:#{spec.port}",
           log_file: spec.log_file,
           server_dir: spec.server_dir
         }}
      end)

    %{state | servers: servers}
  end

  defp start_all_server_processes(state, specs) do
    Enum.reduce_while(specs, {:ok, state}, fn spec, {:ok, acc} ->
      case ProcessSupervisor.start_server(spec_to_server_opts(spec, state.config)) do
        {:ok, pid} ->
          updated_server = %{acc.servers[spec.id] | server_pid: pid, launch_spec: spec}
          {:cont, {:ok, %{acc | servers: Map.put(acc.servers, spec.id, updated_server)}}}

        {:error, reason} ->
          {:halt, {:error, reason}}
      end
    end)
  end

  defp deploy_role_groups(state, specs, deadline) do
    grouped = Enum.group_by(specs, & &1.role)

    Enum.reduce_while(
      @role_deploy_order,
      {:ok, state},
      fn role, {:ok, acc} ->
        case Map.get(grouped, role) do
          nil ->
            {:cont, {:ok, acc}}

          role_specs ->
            server_ids = Enum.map(role_specs, & &1.id)

            with {:ok, acc} <- launch_group(acc, server_ids, role_opts(role), deadline),
                 {:ok, acc} <- post_group_hook(acc, role, deadline) do
              {:cont, {:ok, acc}}
            else
              err -> {:halt, err}
            end
        end
      end
    )
  end

  defp role_opts(:agent), do: [health_check: false]
  defp role_opts(_role), do: [health_check: true]

  defp post_group_hook(state, :agent, deadline) do
    Logger.info("#{state.id}: waiting for agency consensus")

    endpoints_for_role(state, :agent)
    |> Health.wait_for_agency_ready(timeout: remaining_ms(deadline))
    |> case do
      :ok -> {:ok, state}
      error -> error
    end
  end

  defp post_group_hook(state, _role, _deadline), do: {:ok, state}

  defp launch_group(state, server_ids, opts, deadline) do
    health_check? = Keyword.get(opts, :health_check, false)
    timeout = remaining_ms(deadline)
    count = length(server_ids)
    deployment_id = state.id

    role_label = state.servers[hd(server_ids)].role

    Logger.info("#{state.id}: launching #{role_label}s")

    results =
      Task.async_stream(
        server_ids,
        &launch_server_process(state.servers[&1], &1, health_check?, timeout, deployment_id),
        ordered: false,
        max_concurrency: count,
        timeout: timeout + @task_stream_buffer
      )
      |> Enum.to_list()

    collect_launch_results(results, state)
  end

  defp launch_server_process(server, server_id, health_check?, timeout, deployment_id) do
    with :ok <- ServerProcess.launch(server.server_pid),
         os_pid = ServerProcess.os_pid(server.server_pid),
         :ok <- notify_server_started(server_id, server, os_pid, deployment_id),
         :ok <- maybe_health_check(server, health_check?, timeout) do
      {:ok, {server_id, os_pid}}
    end
  end

  defp notify_server_started(server_id, server, os_pid, deployment_id) do
    Logger.info("#{server_id}: started (os_pid=#{os_pid}), endpoint=#{server.endpoint}")

    EventStore.notify(%{
      event: :server_started,
      deployment_id: deployment_id,
      server_id: server_id,
      pid: os_pid,
      timestamp: Toast.get_timestamp()
    })

    :ok
  end

  defp notify_server_stopped(server_id, server, deployment_id) do
    EventStore.notify(%{
      event: :server_stopped,
      deployment_id: deployment_id,
      server_id: server_id,
      pid: server.pid,
      reason: nil,
      timestamp: Toast.get_timestamp()
    })
  end

  defp maybe_health_check(_server, false, _timeout), do: :ok

  defp maybe_health_check(server, true, timeout) do
    process_check_fn = fn -> ServerProcess.status(server.server_pid) == :running end

    Health.wait_until_ready(server.endpoint,
      timeout: timeout,
      process_check_fn: process_check_fn
    )
  end

  defp collect_launch_results(results, state) do
    errors = for {:ok, {:error, reason}} <- results, do: reason
    timeouts = for {:exit, :timeout} <- results, do: :timeout

    case errors ++ timeouts do
      [] ->
        servers =
          Enum.reduce(results, state.servers, fn {:ok, {:ok, {id, os_pid}}}, servers ->
            Map.update!(servers, id, &%{&1 | pid: os_pid})
          end)

        {:ok, %{state | servers: servers}}

      [first | _] ->
        {:error, first}
    end
  end

  defp start_all_health_monitors(state) do
    all_ids = Map.keys(state.servers)
    Logger.debug("Starting health monitors for #{length(all_ids)} servers")

    Enum.reduce_while(all_ids, {:ok, state}, fn server_id, {:ok, acc} ->
      server = acc.servers[server_id]

      case start_single_health_monitor(server_id, server.endpoint) do
        {:ok, pid} ->
          updated = %{server | health_monitor: pid}
          {:cont, {:ok, %{acc | servers: Map.put(acc.servers, server_id, updated)}}}

        {:error, reason} ->
          {:halt, {:error, reason}}
      end
    end)
  end

  defp post_deploy(state) do
    case endpoints_for_role(state, :coordinator) do
      [] -> state
      [endpoint | _] -> fetch_arango_ids(state, endpoint)
    end
  end

  # --- Shutdown ---

  defp do_shutdown(%{status: :failed} = state, timeout) do
    shutdown_servers(state, timeout)
  end

  defp do_shutdown(state, timeout) do
    Logger.debug("Shutting down deployment #{state.id}")
    shutdown_servers(%{state | status: :stopping}, timeout)
  end

  defp shutdown_servers(state, timeout) do
    deadline = System.monotonic_time(:millisecond) + timeout
    stop_all_health_monitors(state)

    grouped = Enum.group_by(state.servers, fn {_id, s} -> s.role end)

    escalated =
      Enum.flat_map(Enum.reverse(@role_deploy_order), fn role ->
        case Map.get(grouped, role) do
          nil ->
            []

          servers ->
            ids = Enum.map(servers, fn {id, _} -> id end)
            Logger.debug("#{state.id}: stopping #{role}s")
            stop_server_group(ids, state, remaining_ms(deadline))
        end
      end)

    record_shutdown_escalations(state.id, escalated)

    notify_event(state, :deployment_stopped)

    %{state | status: :stopped, servers: clear_server_pids(state.servers)}
  end

  defp stop_server_group(server_ids, state, timeout) do
    Task.async_stream(
      server_ids,
      &stop_server_process(state, &1, timeout),
      ordered: false,
      timeout: timeout + ServerProcess.escalation_overhead() + @task_stream_buffer
    )
    |> Enum.flat_map(fn
      {:ok, {:escalated, info}} -> [info]
      _ -> []
    end)
  end

  # --- Rollback ---

  defp rollback(state, reason) do
    Logger.debug("Rolling back #{state.id} due to: #{inspect(reason)}")
    stop_all_health_monitors(state)
    all_ids = Map.keys(state.servers)
    stop_server_group(all_ids, state, state.config.shutdown_timeout)
    Logger.debug("Rollback complete for #{state.id}")

    %{
      state
      | status: :failed,
        servers: clear_server_pids(state.servers),
        error: reason
    }
  end

  # --- Abort all servers ---

  @abort_timeout 60_000

  defp do_abort_all_servers(state) do
    running_servers =
      Enum.filter(state.servers, fn {_id, server} ->
        server.operational_state in [:running, :paused]
      end)

    expected_crashes =
      Enum.reduce(running_servers, state.expected_crashes, fn {server_id, server}, acc ->
        case ServerLifecycle.expect_crash(server_id, @abort_timeout, acc, server) do
          {:ok, updated} -> updated
          {:error, :already_expected} -> acc
        end
      end)

    killed_servers =
      Enum.map(running_servers, fn {server_id, server} ->
        ServerLifecycle.abort_server(server)

        %{server_id: server_id, os_pid: server.pid, log_file: server.log_file}
      end)

    {killed_servers, %{state | expected_crashes: expected_crashes}}
  end

  # --- Control operations ---

  defp do_stop_server(server_id, acc) do
    server_control(acc, server_id, :running,
      action: fn server ->
        ServerLifecycle.stop_server(server, timeout_factor: acc.config.timeout_factor)
      end,
      state: [operational_state: :stopped, expecting_exit: true],
      event: :server_stopped,
      event_extra: fn server -> %{pid: server.pid, reason: nil} end
    )
  end

  defp do_kill_server(server_id, acc) do
    server_control(acc, server_id, :running,
      action: &ServerLifecycle.kill_server/1,
      state: [operational_state: :killed, expecting_exit: true],
      event: :server_killed,
      event_extra: fn server -> %{pid: server.pid} end
    )
  end

  defp do_pause_server(server_id, acc) do
    server_control(acc, server_id, :running,
      action: &ServerLifecycle.pause_server/1,
      state: [operational_state: :paused, expecting_exit: false],
      event: :server_paused
    )
  end

  defp do_resume_server(server_id, acc) do
    server_control(acc, server_id, :paused,
      action: &ServerLifecycle.resume_server/1,
      state: [operational_state: :running, expecting_exit: false],
      event: :server_resumed
    )
  end

  defp server_control(acc, server_id, required_state, opts) do
    with {:ok, server} <- fetch_server(acc, server_id),
         :ok <- ServerLifecycle.require_state(server, required_state) do
      opts[:action].(server)
      acc = update_server(acc, server_id, opts[:state])

      extra = if opts[:event_extra], do: opts[:event_extra].(server), else: %{}

      notify_event(acc, opts[:event], Map.merge(%{server_id: server_id}, extra))

      {:ok, acc}
    end
  end

  defp do_restart_server(server_id, acc, opts) do
    with {:ok, server} <- fetch_server(acc, server_id) do
      ServerLifecycle.stop_before_restart(server, timeout_factor: acc.config.timeout_factor)
      notify_server_stopped(server_id, server, acc.id)

      acc =
        update_server(acc, server_id, operational_state: :stopped, expecting_exit: true)

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

    with :ok <- ServerLifecycle.relaunch_and_wait(server, opts) do
      new_pid = ServerProcess.os_pid(server.server_pid)
      notify_server_started(server_id, server, new_pid, acc.id)

      acc =
        update_server(acc, server_id,
          operational_state: :running,
          expecting_exit: false
        )

      {:ok, acc}
    end
  end

  defp do_expect_crash(server_id, acc, timeout) do
    with {:ok, server} <- fetch_server(acc, server_id),
         {:ok, expected_crashes} <-
           ServerLifecycle.expect_crash(server_id, timeout, acc.expected_crashes, server) do
      {:ok, %{acc | expected_crashes: expected_crashes}}
    end
  end

  # --- Status derivation ---

  defp derive_status(servers) do
    Enum.reduce(servers, :ready, fn
      _server, :failed ->
        :failed

      {_id, server}, acc ->
        cond do
          ServerInstance.unexpected_crash?(server) -> :failed
          server.operational_state == :running -> acc
          true -> :degraded
        end
    end)
  end

  defp derive_mode(state) do
    if Enum.any?(state.servers, fn {_id, s} -> ServerInstance.cluster_role?(s.role) end) do
      :cluster
    else
      :single_server
    end
  end

  # --- Target resolution ---

  defp resolve_target(state, server_id) when is_binary(server_id) do
    resolve_target_by_id(state, server_id)
  end

  defp resolve_target(state, role: role) when is_atom(role) do
    case for({id, %{role: ^role}} <- state.servers, do: id) do
      [] -> {:error, {:no_servers_for_role, role}}
      ids -> {:ok, ids}
    end
  end

  defp resolve_target(state, role: role, index: index) when is_atom(role) and is_integer(index) do
    ids = for({id, %{role: ^role}} <- state.servers, do: id) |> Enum.sort()

    case Enum.at(ids, index) do
      nil -> {:error, {:no_server_at_index, role, index}}
      id -> {:ok, [id]}
    end
  end

  defp resolve_target(state, arango_id: arango_id) when is_binary(arango_id) do
    case Enum.find(state.servers, fn {_id, s} -> s.arango_id == arango_id end) do
      {toast_id, _} -> {:ok, [toast_id]}
      nil -> {:error, :not_found}
    end
  end

  defp resolve_target(_state, target), do: {:error, {:invalid_target, target}}

  # --- Info ---

  defp do_build_info(state) do
    %{
      id: state.id,
      status: state.status,
      servers: state.servers,
      error: state.error,
      agency_dump: state.agency_dump
    }
  end

  # --- Cluster-specific helpers ---

  defp fetch_arango_ids(state, endpoint) do
    url = "#{endpoint}/_admin/cluster/health"

    case Req.get(url) do
      {:ok, %{status: 200, body: body}} ->
        apply_arango_id_mapping(state, Map.get(body, "Health", %{}))

      other ->
        Logger.warning("Failed to fetch cluster health for arango ID mapping: #{inspect(other)}")
        state
    end
  rescue
    e ->
      Logger.warning("Failed to fetch arango ID mapping: #{Exception.message(e)}")
      state
  end

  defp apply_arango_id_mapping(state, health) do
    Enum.reduce(health, state, fn {arango_id, info}, acc ->
      case find_toast_id_by_endpoint(acc, Map.get(info, "Endpoint", "")) do
        nil ->
          acc

        toast_id ->
          acc = update_server(acc, toast_id, arango_id: arango_id)

          notify_event(acc, :server_identified, %{server_id: toast_id, arango_id: arango_id})

          acc
      end
    end)
  end

  defp find_toast_id_by_endpoint(state, endpoint) do
    with {:ok, port} <- extract_port(endpoint) do
      Enum.find_value(state.servers, fn {toast_id, server} ->
        if server.port == port, do: toast_id
      end)
    end
  end

  defp extract_port(endpoint) do
    case URI.parse(endpoint) do
      %URI{port: port} when is_integer(port) -> {:ok, port}
      _ -> :error
    end
  end

  defp get_living_agents(state) do
    for {_id, server} <- state.servers,
        server.role == :agent,
        server.operational_state in [:running, nil] do
      %{id: server.id, endpoint: server.endpoint}
    end
  end

  # --- Control helpers ---

  defp resolve_and_apply(state, target, fun) do
    case resolve_target(state, target) do
      {:ok, server_ids} -> apply_to_each(server_ids, state, fun)
      {:error, _} = err -> {:reply, err, state}
    end
  end

  defp apply_to_each(server_ids, state, fun) do
    server_ids
    |> Enum.reduce_while({:ok, state}, fn server_id, {:ok, acc} ->
      case fun.(server_id, acc) do
        {:ok, new_acc} -> {:cont, {:ok, new_acc}}
        {:error, _} = err -> {:halt, {err, acc}}
      end
    end)
    |> case do
      {:ok, final_state} ->
        final_state = %{final_state | status: derive_status(final_state.servers)}
        {:reply, :ok, final_state}

      {{:error, _} = err, partial_state} ->
        partial_state = %{partial_state | status: derive_status(partial_state.servers)}
        {:reply, err, partial_state}
    end
  end

  defp find_server_by_health_monitor(state, pid) do
    Enum.find(state.servers, fn {_id, server} -> server.health_monitor == pid end)
  end

  defp endpoints_for_role(state, role) do
    state.servers
    |> Enum.filter(fn {_id, s} -> s.role == role end)
    |> Enum.sort_by(fn {id, _} -> id end)
    |> Enum.map(fn {_id, s} -> s.endpoint end)
  end

  # --- Server state helpers ---

  defp fetch_server(state, server_id) do
    with :error <- Map.fetch(state.servers, server_id), do: {:error, :not_found}
  end

  defp update_server(state, server_id, updates) do
    %{state | servers: Map.update!(state.servers, server_id, &struct!(&1, updates))}
  end

  defp start_single_health_monitor(server_id, endpoint) do
    case ProcessSupervisor.start_health_monitor(
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

  defp stop_all_health_monitors(state) do
    for {_id, server} <- state.servers do
      ServerLifecycle.stop_health_monitor(server)
    end

    :ok
  end

  defp stop_health_monitor(state, server_id) do
    %ServerInstance{} = server = state.servers[server_id]
    ServerLifecycle.stop_health_monitor(server)
  end

  defp stop_server_process(state, server_id, timeout) do
    Logger.debug("Stopping server process for #{server_id}")

    case server = Map.fetch!(state.servers, server_id) do
      %ServerInstance{server_pid: pid} when pid != nil ->
        result = ServerProcess.stop(pid, timeout)
        DynamicSupervisor.terminate_child(ProcessSupervisor, pid)
        notify_server_stopped(server_id, server, state.id)

        case result do
          :escalated ->
            {:escalated, %{server_id: server_id, os_pid: server.pid, log_file: server.log_file}}

          _ ->
            :ok
        end

      _ ->
        :ok
    end
  catch
    :exit, _ -> :ok
  end

  defp spec_to_server_opts(spec, config) do
    handler = if config.show_server_logs, do: &ServerLifecycle.print_server_output/2

    [
      id: spec.id,
      executable: spec.executable,
      args: spec.args,
      env: spec.env,
      working_dir: spec.working_dir,
      listener: self(),
      output_handler: handler
    ]
  end

  defp clear_server_pids(servers) do
    Map.new(servers, fn {id, server} -> {id, %{server | server_pid: nil, health_monitor: nil}} end)
  end

  defp handle_deploy_failure(state, reason, rollback_fn) do
    Logger.error("Deploy failed for #{state.id}: #{inspect(reason)}")
    {killed_servers, state} = do_abort_all_servers(state)

    if reason == :timeout do
      ToastTest.EventStore.record_timeout_kill(
        :startup_timeout,
        "Startup timeout — deployment did not become ready in time",
        killed_servers
      )
    end

    {:error, reason, rollback_fn.(state, reason)}
  end

  defp record_shutdown_escalations(_id, []), do: :ok

  defp record_shutdown_escalations(id, escalated) do
    Logger.warning("#{id}: #{length(escalated)} server(s) required shutdown escalation")

    ToastTest.EventStore.record_timeout_kill(
      :shutdown_timeout,
      "Shutdown timeout — server(s) did not respond to SIGTERM",
      escalated
    )
  end

  defp notify_event(state, event, extra \\ %{}) do
    EventStore.notify(
      Map.merge(%{event: event, deployment_id: state.id, timestamp: Toast.get_timestamp()}, extra)
    )
  end

  defp remaining_ms(deadline) do
    max(0, deadline - System.monotonic_time(:millisecond))
  end

  defp resolve_target_by_id(state, server_id) do
    if Map.has_key?(state.servers, server_id),
      do: {:ok, [server_id]},
      else: {:error, :not_found}
  end
end
