################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule Toast.Deployment.Controller do
  @moduledoc "GenServer orchestrating the lifecycle of an ArangoDB deployment."

  use GenServer

  require Logger

  alias Toast.Deployment.Config

  alias Toast.Deployment.{
    CrashExpectation,
    DefaultEventListener,
    DeployPipeline,
    Events,
    ServerInstance,
    ServerLifecycle,
    ShutdownPipeline
  }

  alias Toast.Diagnostics.AgencyDump
  alias Toast.Process.CrashInfo
  alias Toast.Process.ServerProcess

  @intentional_exit_signals [nil, 15]

  @type status :: :stopped | :starting | :ready | :degraded | :stopping | :failed

  @type deployment_error ::
          {:server_crashed, String.t(), Toast.Process.CrashInfo.t()}
          | {:server_unhealthy, String.t()}
          | nil

  defmodule State do
    @moduledoc false
    @enforce_keys [:config]
    defstruct [
      :config,
      :id,
      :error,
      :jwt_provider,
      status: :stopped,
      servers: %{},
      expected_crashes: %{},
      crash_waiters: %{},
      agency_dump: nil,
      event_listener: Toast.Deployment.DefaultEventListener
    ]

    @type t :: %__MODULE__{
            config: Toast.Deployment.Config.t(),
            id: String.t() | nil,
            error: Toast.Deployment.Controller.deployment_error(),
            status: Toast.Deployment.Controller.status(),
            servers: %{optional(String.t()) => Toast.Deployment.ServerInstance.t()},
            expected_crashes: map(),
            crash_waiters: %{optional(String.t()) => [{GenServer.from(), reference()}]},
            agency_dump: term(),
            event_listener: module(),
            jwt_provider: Toast.JWT.Provider.t() | nil
          }

    @doc false
    @spec update_server(t(), String.t(), keyword()) :: t()
    def update_server(state, server_id, updates) do
      %{state | servers: Map.update!(state.servers, server_id, &struct!(&1, updates))}
    end

    @doc false
    @spec remaining_ms(integer()) :: non_neg_integer()
    def remaining_ms(deadline) do
      max(0, deadline - System.monotonic_time(:millisecond))
    end

    @doc false
    @spec role_deploy_order() :: [atom()]
    def role_deploy_order, do: [:single, :agent, :dbserver, :coordinator]

    @doc false
    @spec task_stream_buffer() :: non_neg_integer()
    def task_stream_buffer, do: 5_000
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

  @spec stop_server(GenServer.server(), term()) :: :ok | {:error, term()}
  def stop_server(server, server_id),
    do: GenServer.call(server, {:stop_server, server_id}, :infinity)

  @spec kill_server(GenServer.server(), term()) :: :ok | {:error, term()}
  def kill_server(server, server_id),
    do: GenServer.call(server, {:kill_server, server_id}, :infinity)

  @spec pause_server(GenServer.server(), term()) :: :ok | {:error, term()}
  def pause_server(server, server_id),
    do: GenServer.call(server, {:pause_server, server_id}, :infinity)

  @spec resume_server(GenServer.server(), term()) :: :ok | {:error, term()}
  def resume_server(server, server_id),
    do: GenServer.call(server, {:resume_server, server_id}, :infinity)

  @spec restart_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def restart_server(server, server_id, opts \\ []),
    do: GenServer.call(server, {:restart_server, server_id, opts}, :infinity)

  @spec start_server(GenServer.server(), term(), keyword()) :: :ok | {:error, term()}
  def start_server(server, server_id, opts \\ []),
    do: GenServer.call(server, {:start_server, server_id, opts}, :infinity)

  @spec notify_crash(GenServer.server(), String.t(), CrashInfo.t()) :: :ok
  def notify_crash(server, server_id, crash_info) do
    send(server, {:server_crashed, server_id, crash_info})
    :ok
  end

  @spec expect_crash(GenServer.server(), String.t(), timeout()) :: :ok | {:error, term()}
  def expect_crash(server, server_id, timeout \\ 30_000) do
    GenServer.call(server, {:expect_crash, server_id, timeout})
  end

  @spec verify_crash(GenServer.server(), String.t(), timeout()) ::
          {:ok, CrashInfo.t()} | {:error, term()}
  def verify_crash(server, server_id, timeout \\ 5_000) do
    GenServer.call(server, {:verify_crash, server_id, timeout}, timeout + 5_000)
  end

  @doc """
  Blocks until the server's `operational_state` has transitioned to `:crashed`
  (via the expected or unexpected crash path), or the timeout fires.
  Returns `:ok` in either case; if already `:crashed`, returns immediately.
  """
  @spec await_crash_event(GenServer.server(), String.t(), timeout()) :: :ok | :timeout
  def await_crash_event(server, server_id, timeout) do
    GenServer.call(server, {:await_crash_event, server_id, timeout}, timeout + 5_000)
  end

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    config = Keyword.get(opts, :config, Config.new())
    id = Keyword.fetch!(opts, :id)
    listener = Keyword.get(opts, :event_listener, DefaultEventListener)
    servers = Keyword.get(opts, :servers, %{})
    status = Keyword.get(opts, :status, :stopped)
    jwt_provider = Keyword.get(opts, :jwt_provider)

    # Monitor any pre-existing health monitors (e.g., when servers are passed in
    # for testing) so we receive :DOWN messages if they crash.
    for {_id, %ServerInstance{health_monitor: pid}} when is_pid(pid) <- servers do
      Process.monitor(pid)
    end

    state = %State{
      id: id,
      config: config,
      event_listener: listener,
      servers: servers,
      status: status,
      jwt_provider: jwt_provider
    }

    {:ok, state}
  end

  @impl true
  def handle_call({:deploy, specs, timeout, opts}, _from, %{status: :stopped} = state) do
    state = %{state | status: :starting}

    case DeployPipeline.run(state, specs, timeout, opts) do
      {:ok, new_state} ->
        Logger.debug("Deploy succeeded, status=#{new_state.status}")
        {:reply, :ok, new_state}

      {:error, reason, failed_state} ->
        Logger.debug("Deploy failed: #{inspect(reason)}")
        new_state = ShutdownPipeline.handle_deploy_failure(failed_state, reason)
        {:reply, {:error, reason}, new_state}
    end
  end

  def handle_call({:shutdown, timeout}, _from, state) do
    Logger.info("Shutting down deployment (status=#{state.status}, timeout=#{timeout}ms)")
    new_state = ShutdownPipeline.shutdown(state, timeout)
    Logger.info("Shutdown complete, status=#{new_state.status}")
    {:reply, :ok, new_state}
  end

  def handle_call(:abort, _from, state) do
    Logger.info("Aborting all servers")
    {killed_servers, new_state} = ShutdownPipeline.abort_all(state)
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
    verify_crash(server_id, timeout, state, from)
  end

  def handle_call({:await_crash_event, server_id, timeout}, from, state) do
    case Map.get(state.servers, server_id) do
      %ServerInstance{operational_state: :crashed} ->
        {:reply, :ok, state}

      _ ->
        timer = Process.send_after(self(), {:await_crash_timeout, server_id, from}, timeout)
        waiter = {from, timer}
        new_waiters = Map.update(state.crash_waiters, server_id, [waiter], &[waiter | &1])
        {:noreply, %{state | crash_waiters: new_waiters}}
    end
  end

  def handle_call({:resolve_target, target}, _from, state) do
    {:reply, resolve_target(state, target), state}
  end

  def handle_call(:dump_agency, _from, state) do
    dump =
      AgencyDump.capture(
        endpoints: get_endpoints_for_role(state, :agent),
        auth: Toast.JWT.Provider.maybe_auth(state.jwt_provider)
      )

    {:reply, dump, %{state | agency_dump: dump}}
  end

  @impl true
  def handle_info({:server_crashed, server_id, crash_info}, state) do
    case Map.get(state.servers, server_id) do
      nil ->
        Logger.warning("Received crash notification for unknown server #{server_id}, ignoring")
        {:noreply, state}

      %ServerInstance{} = server ->
        handle_crash_result(handle_crash(server_id, crash_info, server, state))
    end
  end

  def handle_info({:server_unhealthy, server_id}, state) do
    Logger.error("Server #{server_id} is unresponsive, sending SIGABRT for crash backtrace")
    server = state.servers[server_id]
    if server && server.server_pid, do: ServerProcess.send_signal(server.server_pid, :sigabrt)
    state = update_server(state, server_id, operational_state: :killed, expecting_exit: true)
    Logger.debug("Deployment status: #{state.status} -> :failed")
    {:noreply, %{state | status: :failed, error: {:server_unhealthy, server_id}}}
  end

  def handle_info({:expect_crash_timeout, server_id}, state) do
    {:noreply, handle_expect_crash_timeout(server_id, state)}
  end

  def handle_info({:verify_crash_timeout, server_id}, state) do
    {:noreply, handle_verify_crash_timeout(server_id, state)}
  end

  def handle_info({:await_crash_timeout, server_id, from}, state) do
    {:noreply, handle_await_crash_timeout(server_id, from, state)}
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

        case ServerLifecycle.start_health_monitor(server_id, server.endpoint,
               jwt_provider: state.jwt_provider
             ) do
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

  defp handle_crash_result(result) do
    case result do
      {:expected, state} ->
        {:noreply, %{state | status: ServerInstance.derive_cluster_status(state.servers)}}

      {:intentional_exit, state} ->
        {:noreply, state}

      {:failed, state} ->
        {:noreply, state}
    end
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
    if is_map_key(acc.expected_crashes, server_id) do
      {:error,
       {:conflict,
        "kill_server conflicts with a pending expect_crash for #{server_id}. " <>
          "kill_server is an external kill that suppresses crash notifications, " <>
          "so verify_crash would never complete. Use kill_server without " <>
          "expect_crash, or trigger the crash from within the server (e.g., via a failure point)."}}
    else
      server_control(acc, server_id, :running,
        action: &ServerLifecycle.kill_server/1,
        state: [operational_state: :killed, expecting_exit: true],
        event: :server_killed,
        event_extra: fn server -> %{pid: server.pid} end
      )
    end
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

      Events.notify(
        acc.event_listener,
        acc,
        opts[:event],
        Map.merge(%{server_id: server_id}, extra)
      )

      {:ok, acc}
    end
  end

  defp do_restart_server(server_id, acc, opts) do
    with {:ok, server} <- fetch_server(acc, server_id) do
      ServerLifecycle.stop_before_restart(server, timeout_factor: acc.config.timeout_factor)
      Events.server_stopped(acc.event_listener, server_id, server, acc.id)

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
    opts =
      opts
      |> Keyword.put_new(:timeout_factor, acc.config.timeout_factor)
      |> Keyword.put_new(:jwt_provider, acc.jwt_provider)

    with :ok <- ServerLifecycle.relaunch_and_wait(server, opts) do
      new_pid = ServerProcess.os_pid(server.server_pid)
      Events.server_started(acc.event_listener, server_id, server, new_pid, acc.id)

      acc =
        update_server(acc, server_id,
          operational_state: :running,
          expecting_exit: false
        )

      {:ok, acc}
    end
  end

  defp do_expect_crash(server_id, acc, timeout) do
    with {:ok, server} <- fetch_server(acc, server_id) do
      expect_crash(server_id, timeout, server, acc)
    end
  end

  # --- Crash handling ---

  defp handle_crash(server_id, crash_info, server, state) do
    case Map.get(state.expected_crashes, server_id) do
      %CrashExpectation{} = entry ->
        handle_expected_crash(server_id, crash_info, entry, state)

      nil ->
        handle_unexpected_crash(server_id, crash_info, server, state)
    end
  end

  defp handle_expected_crash(server_id, crash_info, entry, state) do
    Logger.info("Server #{server_id} crashed as expected")
    notify_crash_event(state, server_id, crash_info, true)

    state =
      update_server(state, server_id, operational_state: :crashed, expecting_exit: true)

    state = release_crash_waiters(state, server_id)

    case entry.waiter do
      {from, verify_timer} ->
        Process.cancel_timer(verify_timer)
        Process.cancel_timer(entry.timer)
        GenServer.reply(from, {:ok, crash_info})
        {:expected, %{state | expected_crashes: Map.delete(state.expected_crashes, server_id)}}

      nil ->
        entry = %{entry | crash_info: crash_info}

        {:expected,
         %{state | expected_crashes: Map.put(state.expected_crashes, server_id, entry)}}
    end
  end

  defp handle_unexpected_crash(
         server_id,
         %{signal: signal} = _crash_info,
         %ServerInstance{expecting_exit: true},
         state
       )
       when signal in @intentional_exit_signals do
    Logger.debug("Server #{server_id} exited intentionally (signal=#{inspect(signal)})")

    {:intentional_exit, state}
  end

  defp handle_unexpected_crash(
         server_id,
         crash_info,
         %ServerInstance{expecting_exit: true},
         state
       ) do
    Logger.error(
      "Server #{server_id} crashed unexpectedly during intentional stop: #{inspect(crash_info)}"
    )

    fail_server(server_id, crash_info, state)
  end

  defp handle_unexpected_crash(server_id, crash_info, _server, state) do
    Logger.error("Server #{server_id} crashed: #{inspect(crash_info)}")
    fail_server(server_id, crash_info, state)
  end

  defp fail_server(server_id, crash_info, state) do
    notify_crash_event(state, server_id, crash_info, false)
    stop_health_monitor(state, server_id)

    state =
      update_server(state, server_id,
        operational_state: :crashed,
        expecting_exit: false,
        health_monitor: nil
      )

    state = release_crash_waiters(state, server_id)

    {:failed, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}
  end

  defp release_crash_waiters(state, server_id) do
    case Map.pop(state.crash_waiters, server_id) do
      {nil, waiters} ->
        %{state | crash_waiters: waiters}

      {entries, waiters} ->
        for {from, timer} <- entries do
          Process.cancel_timer(timer)
          GenServer.reply(from, :ok)
        end

        %{state | crash_waiters: waiters}
    end
  end

  defp handle_await_crash_timeout(server_id, from, state) do
    entries = Map.get(state.crash_waiters, server_id, [])

    case List.keyfind(entries, from, 0) do
      nil ->
        state

      {^from, _timer} ->
        GenServer.reply(from, :timeout)
        remaining = List.keydelete(entries, from, 0)

        new_waiters =
          if remaining == [],
            do: Map.delete(state.crash_waiters, server_id),
            else: Map.put(state.crash_waiters, server_id, remaining)

        %{state | crash_waiters: new_waiters}
    end
  end

  defp notify_crash_event(state, server_id, crash_info, expected) do
    state.event_listener.on_event(%{
      event: :server_crashed,
      deployment_id: state.id,
      server_id: server_id,
      pid: crash_info.os_pid,
      crash_info: crash_info,
      expected: expected,
      timestamp: Toast.get_timestamp()
    })
  end

  # --- Expect / verify crash protocol ---

  defp expect_crash(server_id, timeout, server, state) do
    if is_map_key(state.expected_crashes, server_id) do
      {:error, :already_expected}
    else
      Logger.debug("Registered expected crash for #{server_id} (timeout=#{timeout}ms)")
      ServerLifecycle.suspend_health_monitor(server)
      timer = Process.send_after(self(), {:expect_crash_timeout, server_id}, timeout)
      entry = %CrashExpectation{timer: timer}
      {:ok, %{state | expected_crashes: Map.put(state.expected_crashes, server_id, entry)}}
    end
  end

  defp verify_crash(server_id, timeout, state, from) do
    case Map.get(state.expected_crashes, server_id) do
      nil ->
        {:reply, {:error, :no_expectation}, state}

      %CrashExpectation{crash_info: nil} = entry ->
        server = Map.get(state.servers, server_id)

        if server && server.operational_state == :killed do
          Process.cancel_timer(entry.timer)

          {:reply,
           {:error,
            {:killed_externally,
             "Server #{server_id} was killed via kill_server, which suppresses crash " <>
               "notifications. verify_crash only works with server-initiated crashes " <>
               "(e.g., triggered by a failure point)."}},
           %{state | expected_crashes: Map.delete(state.expected_crashes, server_id)}}
        else
          verify_timer =
            Process.send_after(self(), {:verify_crash_timeout, server_id}, timeout)

          entry = %{entry | waiter: {from, verify_timer}}

          {:noreply,
           %{state | expected_crashes: Map.put(state.expected_crashes, server_id, entry)}}
        end

      %CrashExpectation{crash_info: crash_info, timer: timer} ->
        Process.cancel_timer(timer)

        {:reply, {:ok, crash_info},
         %{state | expected_crashes: Map.delete(state.expected_crashes, server_id)}}
    end
  end

  defp handle_expect_crash_timeout(server_id, state) do
    case Map.get(state.expected_crashes, server_id) do
      %CrashExpectation{crash_info: nil} = entry ->
        Logger.warning("Expected crash for #{server_id} timed out")
        notify_waiter_timeout(entry)
        server = Map.get(state.servers, server_id)
        ServerLifecycle.resume_health_monitor(server)
        %{state | expected_crashes: Map.delete(state.expected_crashes, server_id)}

      _ ->
        state
    end
  end

  defp handle_verify_crash_timeout(server_id, state) do
    case Map.get(state.expected_crashes, server_id) do
      %CrashExpectation{waiter: {from, _}} ->
        GenServer.reply(from, {:error, :timeout})
        server = Map.get(state.servers, server_id)
        ServerLifecycle.resume_health_monitor(server)
        %{state | expected_crashes: Map.delete(state.expected_crashes, server_id)}

      _ ->
        state
    end
  end

  defp notify_waiter_timeout(%{waiter: {from, verify_timer}}) do
    Process.cancel_timer(verify_timer)
    GenServer.reply(from, {:error, :timeout})
  end

  defp notify_waiter_timeout(_), do: :ok

  # --- Server state helpers ---

  defp fetch_server(state, server_id) do
    with :error <- Map.fetch(state.servers, server_id), do: {:error, :not_found}
  end

  defp update_server(state, server_id, updates) do
    State.update_server(state, server_id, updates)
  end

  defp stop_health_monitor(state, server_id) do
    %ServerInstance{} = server = state.servers[server_id]
    ServerLifecycle.stop_health_monitor(server)
  end

  defp find_server_by_health_monitor(state, pid) do
    Enum.find(state.servers, fn {_id, server} -> server.health_monitor == pid end)
  end

  defp get_endpoints_for_role(state, role) do
    for {_id, server} <- state.servers,
        server.role == role,
        server.operational_state in [:running, nil],
        server.endpoint != nil do
      server.endpoint
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

  defp resolve_target_by_id(state, server_id) do
    if Map.has_key?(state.servers, server_id),
      do: {:ok, [server_id]},
      else: {:error, :not_found}
  end

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
        final_state = %{
          final_state
          | status: ServerInstance.derive_cluster_status(final_state.servers)
        }

        {:reply, :ok, final_state}

      {{:error, _} = err, partial_state} ->
        partial_state = %{
          partial_state
          | status: ServerInstance.derive_cluster_status(partial_state.servers)
        }

        {:reply, err, partial_state}
    end
  end

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
end
