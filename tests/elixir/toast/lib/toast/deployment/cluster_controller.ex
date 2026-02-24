defmodule Toast.Deployment.ClusterController do
  @moduledoc "GenServer orchestrating the lifecycle of an ArangoDB cluster deployment."

  use GenServer

  require Logger

  alias Toast.Config
  alias Toast.Process.ServerProcess
  alias Toast.Deployment.{Factory, Health, ServerInstance}
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
      on_crash: Keyword.get(opts, :on_crash),
      on_event: Keyword.get(opts, :on_event),
      cluster_id_mapping: %{}
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

    info = %{
      id: state.id,
      status: state.status,
      coordinator_endpoint: coordinator_endpoint,
      servers: state.servers,
      error: state.error,
      diagnostics: state.diagnostics
    }

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

  def handle_call({:cluster_id, toast_id}, _from, state) do
    result =
      case Map.get(state.cluster_id_mapping, toast_id) do
        nil -> {:error, :not_found}
        cluster_id -> {:ok, cluster_id}
      end

    {:reply, result, state}
  end

  def handle_call({:server_by_cluster_id, cluster_internal_id}, _from, state) do
    result =
      case Enum.find(state.cluster_id_mapping, fn {_toast_id, cid} -> cid == cluster_internal_id end) do
        {toast_id, _} ->
          case Map.get(state.servers, toast_id) do
            nil -> {:error, :not_found}
            server -> {:ok, server}
          end

        nil ->
          {:error, :not_found}
      end

    {:reply, result, state}
  end

  @impl true
  def handle_info({:server_crashed, server_id, crash_info}, state) do
    Logger.error("Server #{server_id} crashed: #{inspect(crash_info)}")
    stop_health_monitor(state, server_id)
    notify_crash(state.on_crash, crash_info)
    notify_event(state.on_event, {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()})
    {:noreply, %{state | status: :failed, error: {:server_crashed, server_id, crash_info}}}
  end

  def handle_info({:server_unhealthy, server_id}, state) do
    Logger.error("Server #{server_id} is unresponsive, killing process")
    stop_server_process(state, server_id, 5_000)
    crash_info = %{exit_status: nil, signal: nil, timestamp: DateTime.utc_now()}
    notify_crash(state.on_crash, crash_info)
    notify_event(state.on_event, {:server_crashed, server_id, nil, crash_info, DateTime.utc_now()})
    {:noreply, %{state | status: :failed, error: {:server_unhealthy, server_id}}}
  end

  def handle_info({:DOWN, _ref, :process, pid, reason}, state) when reason != :normal do
    case find_server_by_health_monitor(state, pid) do
      {server_id, _server} when state.status == :ready ->
        Logger.warning("HealthMonitor for #{server_id} died unexpectedly (#{inspect(reason)}), restarting")
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

  # --- Deploy sequence ---

  defp do_deploy(state, timeout) do
    Logger.debug("Starting deploy for cluster #{state.id} (timeout=#{timeout}ms)")
    state = %{state | status: :starting}
    deadline = System.monotonic_time(:millisecond) + timeout

    with {:ok, topology} <- Factory.build_cluster(state.config, state.id),
         state = init_servers_from_topology(state, topology),
         {:ok, state} <- start_all_server_processes(state, topology),
         _ = Logger.info("#{state.id}: launching agents"),
         {:ok, state} <- launch_servers(state, state.agents, timeout: remaining_ms(deadline)),
         _ = Logger.info("#{state.id}: waiting for agency consensus"),
         :ok <- wait_for_agency(state, deadline),
         _ = Logger.info("#{state.id}: agency ready, launching dbservers"),
         {:ok, state} <-
           launch_servers(state, state.dbservers,
             health_check: true,
             timeout: remaining_ms(deadline)
           ),
         _ = Logger.info("#{state.id}: dbservers ready, launching coordinators"),
         {:ok, state} <-
           launch_servers(state, state.coordinators,
             health_check: true,
             timeout: remaining_ms(deadline)
           ),
         {:ok, state} <- start_all_health_monitors(state) do
      state = fetch_cluster_id_mapping(state)
      Logger.info("Cluster #{state.id} ready")
      {:ok, %{state | status: :ready}}
    else
      {:error, reason} ->
        Logger.error("Deploy failed for #{state.id}: #{inspect(reason)}")

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
         %ServerInstance{
           id: spec.id,
           role: role,
           port: spec.port,
           endpoint: "http://127.0.0.1:#{spec.port}",
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
        listener: self(),
        output_handler: &print_server_output/2
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

  defp launch_servers(state, server_ids, opts) do
    health_check? = Keyword.get(opts, :health_check, false)
    timeout = Keyword.get(opts, :timeout, 60_000)
    count = length(server_ids)
    on_event = state.on_event

    results =
      Task.async_stream(
        server_ids,
        fn server_id ->
          server = state.servers[server_id]

          with :ok <- ServerProcess.launch(server.server_pid) do
            os_pid = ServerProcess.os_pid(server.server_pid)
            Logger.info("#{server_id}: started (os_pid=#{os_pid}), endpoint=#{server.endpoint}")
            notify_event(on_event, {:server_started, server_id, os_pid, DateTime.utc_now()})

            if health_check? do
              process_check_fn = fn -> ServerProcess.status(server.server_pid) == :running end

              Health.wait_until_ready(server.endpoint,
                timeout: timeout,
                process_check_fn: process_check_fn
              )
            else
              :ok
            end
            |> case do
              :ok -> {:ok, {server_id, os_pid}}
              error -> error
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

  defp wait_for_agency(state, deadline) do
    agent_endpoints =
      Enum.map(state.agents, fn id -> state.servers[id].endpoint end)

    Health.wait_for_agency_ready(agent_endpoints, timeout: remaining_ms(deadline))
  end

  defp fetch_cluster_id_mapping(state) do
    coordinator_endpoint =
      case state.coordinators do
        [first_id | _] -> state.servers[first_id].endpoint
        [] -> nil
      end

    case coordinator_endpoint do
      nil ->
        state

      endpoint ->
        url = "#{endpoint}/_admin/cluster/health"

        case Req.get(url) do
          {:ok, %{status: 200, body: body}} ->
            health = Map.get(body, "Health", %{})

            mapping =
              Enum.reduce(health, %{}, fn {cluster_id, info}, acc ->
                short_name = Map.get(info, "ShortName", "")

                case find_toast_id_by_short_name(state, short_name) do
                  nil -> acc
                  toast_id -> Map.put(acc, toast_id, cluster_id)
                end
              end)

            %{state | cluster_id_mapping: mapping}

          _ ->
            Logger.warning("Failed to fetch cluster health for ID mapping")
            state
        end
    end
  rescue
    e in [Req.Error, Mint.TransportError] ->
      Logger.warning("Failed to fetch cluster ID mapping: #{Exception.message(e)}")
      state
  end

  defp find_toast_id_by_short_name(state, short_name) do
    Enum.find_value(state.servers, fn {toast_id, server} ->
      if server.id == short_name or toast_id == short_name, do: toast_id
    end)
  end

  # --- Shutdown sequence ---

  defp do_shutdown(state, timeout) do
    Logger.debug("Shutting down cluster #{state.id}")
    state = %{state | status: :stopping}
    deadline = System.monotonic_time(:millisecond) + timeout

    stop_all_health_monitors(state)
    Logger.debug("#{state.id}: stopping coordinators")
    stop_servers(state.coordinators, state, remaining_ms(deadline))
    Logger.debug("#{state.id}: stopping dbservers")
    stop_servers(state.dbservers, state, remaining_ms(deadline))
    Logger.debug("#{state.id}: stopping agents")
    stop_servers(state.agents, state, remaining_ms(deadline))
    diagnostics = collect_all_diagnostics(state)
    Logger.debug("#{state.id}: diagnostics collected")

    %{
      state
      | status: :stopped,
        servers: clear_server_pids(state.servers),
        diagnostics: diagnostics
    }
  end

  defp collect_all_diagnostics(state) do
    {crashed_id, crashed_info} = extract_crashed_server(state.error)

    Map.new(state.servers, fn {server_id, server} ->
      sanitizer_errors = Sanitizer.collect_errors(server.server_dir, server_id)
      log_content = Toast.Utils.Filesystem.read_file_or_nil(server.log_file)

      server_error =
        if server_id == crashed_id,
          do: {:server_crashed, crashed_info},
          else: nil

      diagnostics = %{
        sanitizer_errors: sanitizer_errors,
        server_log: if(log_content, do: ServerLog.scan(log_content)),
        crash_report: if(log_content, do: CrashLogParser.parse(log_content)),
        server_error: server_error,
        server: server
      }

      {server_id, diagnostics}
    end)
  end

  defp extract_crashed_server({:server_crashed, server_id, crash_info}),
    do: {server_id, crash_info}

  defp extract_crashed_server({:server_unhealthy, server_id}),
    do: {server_id, nil}

  defp extract_crashed_server(_), do: {nil, nil}

  defp stop_servers(server_ids, state, timeout) do
    on_event = state.on_event

    Task.async_stream(
      server_ids,
      fn server_id ->
        server = state.servers[server_id]

        if server.server_pid do
          try do
            ServerProcess.stop(server.server_pid, timeout)
            DynamicSupervisor.terminate_child(Toast.Process.Supervisor, server.server_pid)
            notify_event(on_event, {:server_stopped, server_id, server.pid, nil, DateTime.utc_now()})
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

  # --- Rollback on deploy failure ---

  defp rollback(state, reason) do
    Logger.debug("Rolling back #{state.id} due to: #{inspect(reason)}")
    stop_all_health_monitors(state)
    all_ids = state.agents ++ state.dbservers ++ state.coordinators
    stop_servers(all_ids, state, 5_000)
    %{state | status: :failed, servers: clear_server_pids(state.servers), error: reason}
  end

  defp clear_server_pids(servers) do
    Map.new(servers, fn {id, server} -> {id, %{server | server_pid: nil, health_monitor: nil}} end)
  end

  # --- Health monitoring ---

  defp start_all_health_monitors(state) do
    all_ids = state.agents ++ state.dbservers ++ state.coordinators

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

  defp start_single_health_monitor(server_id, endpoint) do
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

  defp stop_all_health_monitors(state) do
    for {_id, server} <- state.servers, server.health_monitor != nil do
      try do
        Toast.Process.HealthMonitor.stop(server.health_monitor)
      catch
        :exit, _ -> :ok
      end
    end
  end

  defp stop_health_monitor(state, server_id) do
    case state.servers[server_id] do
      %{health_monitor: nil} ->
        :ok

      %{health_monitor: pid} ->
        try do
          Toast.Process.HealthMonitor.stop(pid)
        catch
          :exit, _ -> :ok
        end

      nil ->
        :ok
    end
  end

  defp stop_server_process(state, server_id, timeout) do
    case state.servers[server_id] do
      %{server_pid: nil} ->
        :ok

      %{server_pid: pid} ->
        try do
          ServerProcess.stop(pid, timeout)
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

  # --- Helpers ---

  defp notify_crash(nil, _crash_info), do: :ok
  defp notify_crash(on_crash, crash_info) when is_function(on_crash, 1), do: on_crash.(crash_info)

  defp notify_event(nil, _event), do: :ok
  defp notify_event(on_event, event) when is_function(on_event, 1), do: on_event.(event)

  defp print_server_output(server_id, data) do
    data
    |> String.split("\n")
    |> Enum.reject(&(&1 == ""))
    |> Enum.each(&IO.puts("  #{server_id} | #{&1}"))
  end

  defp generate_id do
    "toast-cluster-#{System.unique_integer([:positive])}"
  end

  defp remaining_ms(deadline) do
    max(0, deadline - System.monotonic_time(:millisecond))
  end
end
