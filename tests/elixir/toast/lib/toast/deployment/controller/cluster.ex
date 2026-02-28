defmodule Toast.Deployment.Controller.Cluster do
  @moduledoc false

  @behaviour Toast.Deployment.Controller

  require Logger

  alias Toast.Process.ServerProcess
  alias Toast.Deployment.{Factory, Health, ServerInstance, ServerLifecycle}
  alias Toast.Deployment.Controller
  alias Toast.Diagnostics.{AgencyDump, CrashLogParser, Sanitizer, ServerLog}

  @impl true
  def init_servers(_id), do: %{}

  @impl true
  def init_mode_state do
    %{
      agents: [],
      dbservers: [],
      coordinators: [],
      cluster_id_mapping: %{},
      agency_dump: nil
    }
  end

  @impl true
  def deploy(state, timeout) do
    Logger.debug("Starting deploy for cluster #{state.id} (timeout=#{timeout}ms)")
    state = %{state | status: :starting}
    deadline = System.monotonic_time(:millisecond) + timeout

    with {:ok, topology} <- Factory.build_cluster(state.config, state.id),
         state = init_servers_from_topology(state, topology),
         {:ok, state} <- start_all_server_processes(state, topology),
         _ = Logger.info("#{state.id}: launching agents"),
         {:ok, state} <-
           launch_servers(state, state.mode_state.agents,
             timeout: Controller.remaining_ms(deadline)
           ),
         _ = Logger.info("#{state.id}: waiting for agency consensus"),
         :ok <- wait_for_agency(state, deadline),
         _ = Logger.info("#{state.id}: agency ready, launching dbservers"),
         {:ok, state} <-
           launch_servers(state, state.mode_state.dbservers,
             health_check: true,
             timeout: Controller.remaining_ms(deadline)
           ),
         _ = Logger.info("#{state.id}: dbservers ready, launching coordinators"),
         {:ok, state} <-
           launch_servers(state, state.mode_state.coordinators,
             health_check: true,
             timeout: Controller.remaining_ms(deadline)
           ),
         {:ok, state} <- start_all_health_monitors(state) do
      state = fetch_cluster_id_mapping(state)
      servers = Map.new(state.servers, fn {id, s} -> {id, %{s | operational_state: :running}} end)
      state = %{state | servers: servers}
      Logger.info("Cluster #{state.id} ready")
      {:ok, %{state | status: :ready}}
    else
      {:error, reason} ->
        Logger.error("Deploy failed for #{state.id}: #{inspect(reason)}")
        failed_state = rollback(state, reason)
        {:error, reason, failed_state}
    end
  end

  @impl true
  def shutdown(%{status: :failed} = state, timeout) do
    do_shutdown(state, timeout)
  end

  def shutdown(state, timeout) do
    Logger.debug("Shutting down cluster #{state.id}")
    state = %{state | status: :stopping}
    do_shutdown(state, timeout)
  end

  @impl true
  def derive_status(servers) do
    server_list = Map.values(servers)
    states = Enum.map(server_list, & &1.operational_state)

    cond do
      Enum.any?(server_list, &ServerInstance.unexpected_crash?/1) ->
        :failed

      Enum.all?(states, &(&1 == :running)) ->
        :ready

      Enum.any?(states, &(&1 in [:stopped, :killed, :paused])) ->
        :degraded

      true ->
        :ready
    end
  end

  @impl true
  def resolve_target(state, server_id) when is_binary(server_id) do
    if Map.has_key?(state.servers, server_id),
      do: {:ok, [server_id]},
      else: {:error, :not_found}
  end

  def resolve_target(state, role: role) when is_atom(role) do
    ids =
      state.servers
      |> Enum.filter(fn {_id, server} -> server.role == role end)
      |> Enum.map(fn {id, _server} -> id end)

    if ids == [],
      do: {:error, {:no_servers_for_role, role}},
      else: {:ok, ids}
  end

  def resolve_target(state, role: role, index: index) when is_atom(role) and is_integer(index) do
    ids =
      state.servers
      |> Enum.filter(fn {_id, server} -> server.role == role end)
      |> Enum.map(fn {id, _server} -> id end)
      |> Enum.sort()

    case Enum.at(ids, index) do
      nil -> {:error, {:no_server_at_index, role, index}}
      id -> {:ok, [id]}
    end
  end

  def resolve_target(state, cluster_id: cluster_internal_id)
      when is_binary(cluster_internal_id) do
    mapping = state.mode_state.cluster_id_mapping

    case Enum.find(mapping, fn {_toast_id, cid} -> cid == cluster_internal_id end) do
      {toast_id, _} ->
        if Map.has_key?(state.servers, toast_id),
          do: {:ok, [toast_id]},
          else: {:error, :not_found}

      nil ->
        {:error, :not_found}
    end
  end

  def resolve_target(_state, target) do
    {:error, {:invalid_target, target}}
  end

  @impl true
  def build_info(state) do
    coordinator_endpoint =
      case state.mode_state.coordinators do
        [first_id | _] -> state.servers[first_id].endpoint
        [] -> nil
      end

    %{
      id: state.id,
      status: state.status,
      coordinator_endpoint: coordinator_endpoint,
      primary_endpoint: coordinator_endpoint,
      servers: state.servers,
      error: state.error,
      diagnostics: state.diagnostics,
      agency_dump: state.mode_state.agency_dump
    }
  end

  @impl true
  def handle_call_extra(:dump_agency, _from, state) do
    agents = get_living_agents(state)
    dump = AgencyDump.capture(agents: agents)
    state = put_in_mode_state(state, :agency_dump, dump)
    {:reply, dump, state}
  end

  def handle_call_extra({:cluster_id, toast_id}, _from, state) do
    result =
      case Map.get(state.mode_state.cluster_id_mapping, toast_id) do
        nil -> {:error, :not_found}
        cluster_id -> {:ok, cluster_id}
      end

    {:reply, result, state}
  end

  def handle_call_extra({:server_by_cluster_id, cluster_internal_id}, _from, state) do
    result =
      case Enum.find(state.mode_state.cluster_id_mapping, fn {_toast_id, cid} ->
             cid == cluster_internal_id
           end) do
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

  def handle_call_extra(_msg, _from, _state), do: :not_handled

  # --- Deploy helpers ---

  defp init_servers_from_topology(state, topology) do
    {servers, agent_ids} = index_specs(topology.agents, :agent)
    {db_servers, dbserver_ids} = index_specs(topology.dbservers, :dbserver)
    {coord_servers, coordinator_ids} = index_specs(topology.coordinators, :coordinator)

    mode_state = %{
      state.mode_state
      | agents: agent_ids,
        dbservers: dbserver_ids,
        coordinators: coordinator_ids
    }

    %{
      state
      | servers: Map.merge(servers, Map.merge(db_servers, coord_servers)),
        mode_state: mode_state
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
        output_handler: &ServerLifecycle.print_server_output/2
      ]

      case Toast.Process.Supervisor.start_server(opts) do
        {:ok, pid} ->
          updated_server = %{acc.servers[spec.id] | server_pid: pid, launch_spec: spec}
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
        &launch_single_server(state.servers[&1], &1, health_check?, timeout, on_event),
        ordered: false,
        max_concurrency: count,
        timeout: timeout + 5_000
      )
      |> Enum.to_list()

    collect_launch_results(results, state)
  end

  defp launch_single_server(server, server_id, health_check?, timeout, on_event) do
    with :ok <- ServerProcess.launch(server.server_pid),
         os_pid = ServerProcess.os_pid(server.server_pid),
         _ = Logger.info("#{server_id}: started (os_pid=#{os_pid}), endpoint=#{server.endpoint}"),
         _ =
           ServerLifecycle.notify_event(
             on_event,
             {:server_started, server_id, os_pid, DateTime.utc_now()}
           ),
         :ok <- maybe_health_check(server, health_check?, timeout) do
      {:ok, {server_id, os_pid}}
    end
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

  defp wait_for_agency(state, deadline) do
    agent_endpoints =
      Enum.map(state.mode_state.agents, fn id -> state.servers[id].endpoint end)

    Health.wait_for_agency_ready(agent_endpoints, timeout: Controller.remaining_ms(deadline))
  end

  defp fetch_cluster_id_mapping(state) do
    case first_coordinator_endpoint(state) do
      nil -> state
      endpoint -> fetch_cluster_id_mapping_from(state, endpoint)
    end
  rescue
    e ->
      Logger.warning("Failed to fetch cluster ID mapping: #{Exception.message(e)}")
      state
  end

  defp first_coordinator_endpoint(state) do
    case state.mode_state.coordinators do
      [first_id | _] -> state.servers[first_id].endpoint
      [] -> nil
    end
  end

  defp fetch_cluster_id_mapping_from(state, endpoint) do
    url = "#{endpoint}/_admin/cluster/health"

    case Req.get(url) do
      {:ok, %{status: 200, body: body}} ->
        mapping = build_id_mapping(state, Map.get(body, "Health", %{}))
        put_in_mode_state(state, :cluster_id_mapping, mapping)

      _ ->
        Logger.warning("Failed to fetch cluster health for ID mapping")
        state
    end
  end

  defp build_id_mapping(state, health) do
    Enum.reduce(health, %{}, fn {cluster_id, info}, acc ->
      short_name = Map.get(info, "ShortName", "")

      case find_toast_id_by_short_name(state, short_name) do
        nil -> acc
        toast_id -> Map.put(acc, toast_id, cluster_id)
      end
    end)
  end

  defp find_toast_id_by_short_name(state, short_name) do
    Enum.find_value(state.servers, fn {toast_id, server} ->
      if server.id == short_name or toast_id == short_name, do: toast_id
    end)
  end

  # --- Shutdown ---

  defp do_shutdown(state, timeout) do
    deadline = System.monotonic_time(:millisecond) + timeout

    Controller.stop_all_health_monitors(state)
    Logger.debug("#{state.id}: stopping coordinators")
    stop_servers(state.mode_state.coordinators, state, Controller.remaining_ms(deadline))
    Logger.debug("#{state.id}: stopping dbservers")
    stop_servers(state.mode_state.dbservers, state, Controller.remaining_ms(deadline))
    Logger.debug("#{state.id}: stopping agents")
    stop_servers(state.mode_state.agents, state, Controller.remaining_ms(deadline))
    diagnostics = collect_all_diagnostics(state)
    Logger.debug("#{state.id}: diagnostics collected")

    %{
      state
      | status: :stopped,
        servers: Controller.clear_server_pids(state.servers),
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

            ServerLifecycle.notify_event(
              on_event,
              {:server_stopped, server_id, server.pid, nil, DateTime.utc_now()}
            )
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

  # --- Rollback ---

  defp rollback(state, reason) do
    Logger.debug("Rolling back #{state.id} due to: #{inspect(reason)}")
    Controller.stop_all_health_monitors(state)
    ms = state.mode_state
    all_ids = ms.agents ++ ms.dbservers ++ ms.coordinators
    stop_servers(all_ids, state, 5_000 * state.config.timeout_factor)

    %{
      state
      | status: :failed,
        servers: Controller.clear_server_pids(state.servers),
        error: reason
    }
  end

  # --- Health monitoring ---

  defp start_all_health_monitors(state) do
    ms = state.mode_state
    all_ids = ms.agents ++ ms.dbservers ++ ms.coordinators

    Enum.reduce_while(all_ids, {:ok, state}, fn server_id, {:ok, acc} ->
      server = acc.servers[server_id]

      case Controller.start_single_health_monitor(server_id, server.endpoint) do
        {:ok, pid} ->
          updated = %{server | health_monitor: pid}
          {:cont, {:ok, %{acc | servers: Map.put(acc.servers, server_id, updated)}}}

        {:error, reason} ->
          {:halt, {:error, reason}}
      end
    end)
  end

  # --- Helpers ---

  defp get_living_agents(state) do
    state.mode_state.agents
    |> Enum.map(fn id -> state.servers[id] end)
    |> Enum.filter(fn server ->
      server && server.operational_state in [:running, nil]
    end)
    |> Enum.map(fn server -> %{id: server.id, endpoint: server.endpoint} end)
  end

  defp put_in_mode_state(state, key, value) do
    %{state | mode_state: Map.put(state.mode_state, key, value)}
  end
end
