defmodule Toast.Deployment.ClusterController do
  @moduledoc "GenServer orchestrating the lifecycle of an ArangoDB cluster deployment."

  use GenServer

  require Logger

  alias Toast.Config
  alias Toast.Process.ServerProcess
  alias Toast.Deployment.{Factory, Health}
  alias Toast.Diagnostics.{CrashLogParser, Sanitizer, ServerLog}

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
  def deploy(server, timeout \\ 120_000) do
    GenServer.call(server, {:deploy, timeout}, timeout + 5_000)
  end

  @spec shutdown(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def shutdown(server, timeout \\ 60_000) do
    GenServer.call(server, {:shutdown, timeout}, timeout + 5_000)
  end

  @spec get_status(GenServer.server()) :: status()
  def get_status(server) do
    GenServer.call(server, :get_status)
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
      servers: %{},
      agents: [],
      dbservers: [],
      coordinators: [],
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
    new_state = do_shutdown(state, timeout)
    {:reply, :ok, new_state}
  end

  def handle_call({:shutdown, _timeout}, _from, state) do
    {:reply, {:error, {:invalid_status, state.status}}, state}
  end

  def handle_call(:get_status, _from, state) do
    {:reply, state.status, state}
  end

  def handle_call(:get_info, _from, state) do
    coordinator_endpoint =
      case state.coordinators do
        [first_id | _] -> state.servers[first_id].endpoint
        [] -> nil
      end

    server_info =
      Map.new(state.servers, fn {id, s} ->
        {id, %{role: s.role, port: s.port, endpoint: s.endpoint, log_file: s.log_file}}
      end)

    info = %{
      id: state.id,
      status: state.status,
      coordinator_endpoint: coordinator_endpoint,
      servers: server_info,
      error: state.error,
      diagnostics: state.diagnostics
    }

    {:reply, info, state}
  end

  @impl true
  def handle_info({:server_crashed, server_id, crash_info}, state) do
    Logger.error(
      "Server #{server_id} crashed: #{inspect(crash_info)}"
    )

    notify_crash_monitor(state.crash_monitor, server_id, crash_info)
    {:noreply, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}
  end

  def handle_info(msg, state) do
    Logger.debug("Unexpected message: #{inspect(msg)}")
    {:noreply, state}
  end

  # --- Deploy sequence ---

  defp do_deploy(state, timeout) do
    Logger.debug("Starting deploy for cluster #{state.id} (timeout=#{timeout}ms)")
    state = %{state | status: :starting}
    deadline = System.monotonic_time(:millisecond) + timeout

    with {:ok, topology} <- Factory.build_cluster(state.config, state.id),
         state = init_servers_from_topology(state, topology),
         {:ok, state} <- start_all_server_processes(state, topology),
         _ = Logger.info("#{state.id}: launching agents"),
         :ok <- launch_servers(state.agents, state, timeout: remaining_ms(deadline)),
         _ = Logger.info("#{state.id}: waiting for agency consensus"),
         :ok <- wait_for_agency(state, deadline),
         _ = Logger.info("#{state.id}: agency ready, launching dbservers"),
         :ok <-
           launch_servers(state.dbservers, state,
             health_check: true,
             timeout: remaining_ms(deadline)
           ),
         _ = Logger.info("#{state.id}: dbservers ready, launching coordinators"),
         :ok <-
           launch_servers(state.coordinators, state,
             health_check: true,
             timeout: remaining_ms(deadline)
           ) do
      Logger.info("Cluster #{state.id} ready")
      {:ok, %{state | status: :ready}}
    else
      {:error, reason} ->
        Logger.error(
          "Deploy failed for #{state.id}: #{inspect(reason)}"
        )

        failed_state = rollback(state, reason)
        {:error, reason, failed_state}
    end
  end

  defp init_servers_from_topology(state, topology) do
    {servers, agent_ids} = index_specs(topology.agents, :agent)
    {db_servers, dbserver_ids} = index_specs(topology.dbservers, :dbserver)
    {coord_servers, coordinator_ids} = index_specs(topology.coordinators, :coordinator)

    %{
      state
      | servers: Map.merge(servers, Map.merge(db_servers, coord_servers)),
        agents: agent_ids,
        dbservers: dbserver_ids,
        coordinators: coordinator_ids
    }
  end

  defp index_specs(specs, role) do
    servers =
      Map.new(specs, fn spec ->
        {spec.id,
         %{
           role: role,
           port: spec.port,
           endpoint: "http://127.0.0.1:#{spec.port}",
           server_pid: nil,
           log_file: spec.log_file,
           server_dir: spec.server_dir
         }}
      end)

    ids = Enum.map(specs, & &1.id)
    {servers, ids}
  end

  defp start_all_server_processes(state, topology) do
    all_specs = topology.agents ++ topology.dbservers ++ topology.coordinators

    Enum.reduce_while(all_specs, {:ok, state}, fn spec, {:ok, acc} ->
      opts = [
        id: spec.id,
        executable: spec.executable,
        args: spec.args,
        env: spec.env,
        working_dir: spec.working_dir,
        listener: self()
      ]

      case Toast.Process.Supervisor.start_server(opts) do
        {:ok, pid} ->
          updated_server = %{acc.servers[spec.id] | server_pid: pid}
          {:cont, {:ok, %{acc | servers: Map.put(acc.servers, spec.id, updated_server)}}}

        {:error, reason} ->
          {:halt, {:error, reason}}
      end
    end)
  end

  defp launch_servers(server_ids, state, opts) do
    health_check? = Keyword.get(opts, :health_check, false)
    timeout = Keyword.get(opts, :timeout, 60_000)
    count = length(server_ids)

    results =
      Task.async_stream(
        server_ids,
        fn server_id ->
          server = state.servers[server_id]

          with :ok <- ServerProcess.launch(server.server_pid) do
            os_pid = ServerProcess.os_pid(server.server_pid)
            Logger.info("#{server_id}: started (os_pid=#{os_pid}), endpoint=#{server.endpoint}")

            if health_check? do
              process_check_fn = fn -> ServerProcess.status(server.server_pid) == :running end

              Health.wait_until_ready(server.endpoint,
                timeout: timeout,
                process_check_fn: process_check_fn
              )
            else
              :ok
            end
          end
        end,
        ordered: false,
        max_concurrency: count,
        timeout: timeout + 5_000
      )
      |> Enum.to_list()

    errors = for {:ok, {:error, reason}} <- results, do: reason
    timeouts = for {:exit, :timeout} <- results, do: :timeout

    case errors ++ timeouts do
      [] -> :ok
      [first | _] -> {:error, first}
    end
  end

  defp wait_for_agency(state, deadline) do
    agent_endpoints =
      Enum.map(state.agents, fn id -> state.servers[id].endpoint end)

    Health.wait_for_agency_ready(agent_endpoints, timeout: remaining_ms(deadline))
  end

  # --- Shutdown sequence ---

  defp do_shutdown(state, timeout) do
    Logger.debug("Shutting down cluster #{state.id}")
    state = %{state | status: :stopping}
    deadline = System.monotonic_time(:millisecond) + timeout

    Logger.debug("#{state.id}: stopping coordinators")
    stop_servers(state.coordinators, state, remaining_ms(deadline))
    Logger.debug("#{state.id}: stopping dbservers")
    stop_servers(state.dbservers, state, remaining_ms(deadline))
    Logger.debug("#{state.id}: stopping agents")
    stop_servers(state.agents, state, remaining_ms(deadline))
    diagnostics = collect_all_diagnostics(state)
    Logger.debug("#{state.id}: diagnostics collected")
    cleanup_all_dirs(state)

    %{state | status: :stopped, servers: clear_server_pids(state.servers), diagnostics: diagnostics}
  end

  defp collect_all_diagnostics(state) do
    Map.new(state.servers, fn {server_id, server} ->
      sanitizer_errors = Sanitizer.collect_errors(server.server_dir, server_id)
      log_content = Toast.Utils.Filesystem.read_file_or_nil(server.log_file)

      diagnostics = %{
        sanitizer_errors: sanitizer_errors,
        server_log: if(log_content, do: ServerLog.scan(log_content)),
        crash_report: if(log_content, do: CrashLogParser.parse(log_content))
      }

      {server_id, diagnostics}
    end)
  end

  defp stop_servers(server_ids, state, timeout) do
    Task.async_stream(
      server_ids,
      fn server_id ->
        server = state.servers[server_id]

        if server.server_pid do
          try do
            ServerProcess.stop(server.server_pid, timeout)
            DynamicSupervisor.terminate_child(Toast.Process.Supervisor, server.server_pid)
          catch
            :exit, _ -> :ok
          end
        end
      end,
      ordered: false,
      timeout: timeout + 5_000
    )
    |> Stream.run()
  end

  defp cleanup_all_dirs(state) do
    Enum.each(state.servers, fn {_id, server} ->
      Toast.Utils.Filesystem.cleanup_server_dirs(server.server_dir)
    end)
  end

  # --- Rollback on deploy failure ---

  defp rollback(state, reason) do
    Logger.debug("Rolling back #{state.id} due to: #{inspect(reason)}")
    all_ids = state.agents ++ state.dbservers ++ state.coordinators
    stop_servers(all_ids, state, 5_000)
    cleanup_all_dirs(state)
    %{state | status: :failed, servers: clear_server_pids(state.servers), error: reason}
  end

  defp clear_server_pids(servers) do
    Map.new(servers, fn {id, server} -> {id, %{server | server_pid: nil}} end)
  end

  # --- Helpers ---

  defp notify_crash_monitor(nil, _id, _info), do: :ok
  defp notify_crash_monitor(pid, id, info), do: send(pid, {:server_crashed, id, info})

  defp generate_id do
    "toast-cluster-#{System.unique_integer([:positive])}"
  end

  defp remaining_ms(deadline) do
    max(0, deadline - System.monotonic_time(:millisecond))
  end
end
