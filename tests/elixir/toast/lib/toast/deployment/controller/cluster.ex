defmodule Toast.Deployment.Controller.Cluster do
  @moduledoc false

  @behaviour Toast.Deployment.Controller

  require Logger

  alias Toast.Deployment.Controller.Helpers
  alias Toast.Deployment.{Factory, Health, ServerInstance, ServerLifecycle}
  alias Toast.Diagnostics.AgencyDump
  alias Toast.Process.ServerProcess

  # Buffer for Task.async_stream to account for scheduling/collection overhead
  # beyond the per-task timeout.
  @task_stream_buffer 5_000

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
         {:ok, state} <- start_all_server_processes(state, topology) do
      launch_and_finalize(state, deadline)
    else
      {:error, reason} ->
        # Pre-launch failure: no OS processes to abort, just rollback GenServers.
        Logger.error("Deploy failed for #{state.id}: #{inspect(reason)}")
        {:error, reason, rollback(state, reason)}
    end
  end

  @impl true
  def shutdown(%{status: :failed} = state, timeout) do
    do_shutdown(state, timeout)
  end

  def shutdown(state, timeout) do
    Logger.debug("Shutting down cluster #{state.id}")
    do_shutdown(%{state | status: :stopping}, timeout)
  end

  @impl true
  def derive_status(servers) do
    server_list = Map.values(servers)

    cond do
      Enum.any?(server_list, &ServerInstance.unexpected_crash?/1) ->
        :failed

      Enum.all?(server_list, &(&1.operational_state == :running)) ->
        :ready

      Enum.any?(server_list, &(&1.operational_state in [:stopped, :killed, :paused, :crashed])) ->
        :degraded

      true ->
        :ready
    end
  end

  @impl true
  def resolve_target(state, server_id) when is_binary(server_id) do
    Helpers.resolve_target_by_id(state, server_id)
  end

  def resolve_target(state, role: role) when is_atom(role) do
    case for({id, %{role: ^role}} <- state.servers, do: id) do
      [] -> {:error, {:no_servers_for_role, role}}
      ids -> {:ok, ids}
    end
  end

  def resolve_target(state, role: role, index: index) when is_atom(role) and is_integer(index) do
    ids =
      for({id, %{role: ^role}} <- state.servers, do: id)
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
      case Map.fetch(state.mode_state.cluster_id_mapping, toast_id) do
        {:ok, _} = ok -> ok
        :error -> {:error, :not_found}
      end

    {:reply, result, state}
  end

  def handle_call_extra({:server_by_cluster_id, cluster_internal_id}, _from, state) do
    result =
      with {toast_id, _} <-
             Enum.find(state.mode_state.cluster_id_mapping, fn {_id, cid} ->
               cid == cluster_internal_id
             end),
           %ServerInstance{} = server <- Map.get(state.servers, toast_id) do
        {:ok, server}
      else
        _ -> {:error, :not_found}
      end

    {:reply, result, state}
  end

  def handle_call_extra(_msg, _from, _state), do: :not_handled

  # --- Deploy failure handling ---

  # State here has server_pids from start_all_server_processes, so
  # abort_all_servers in the else clause can reach all server processes.
  defp launch_and_finalize(state, deadline) do
    with {:ok, state} <- launch_agents(state, deadline),
         :ok <- wait_for_agency(state, deadline),
         {:ok, state} <- launch_dbservers(state, deadline),
         {:ok, state} <- launch_coordinators(state, deadline),
         {:ok, state} <- start_all_health_monitors(state) do
      state = fetch_cluster_id_mapping(state)
      servers = Map.new(state.servers, fn {id, s} -> {id, %{s | operational_state: :running}} end)
      state = %{state | servers: servers}
      Logger.info("Cluster #{state.id} ready")
      {:ok, %{state | status: :ready}}
    else
      {:error, reason} ->
        Helpers.handle_deploy_failure(state, reason, &rollback/2)
    end
  end

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
      | servers: servers |> Map.merge(db_servers) |> Map.merge(coord_servers),
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

  # No deadline — OS process creation is near-instant; health check timeouts dominate.
  defp start_all_server_processes(state, topology) do
    all_specs = topology.agents ++ topology.dbservers ++ topology.coordinators

    Enum.reduce_while(all_specs, {:ok, state}, fn spec, {:ok, acc} ->
      case Toast.Process.Supervisor.start_server(Helpers.spec_to_server_opts(spec)) do
        {:ok, pid} ->
          updated_server = %{acc.servers[spec.id] | server_pid: pid, launch_spec: spec}
          {:cont, {:ok, %{acc | servers: Map.put(acc.servers, spec.id, updated_server)}}}

        {:error, reason} ->
          {:halt, {:error, reason}}
      end
    end)
  end

  defp launch_agents(state, deadline) do
    Logger.info("#{state.id}: launching agents")
    launch_servers(state, state.mode_state.agents, timeout: Helpers.remaining_ms(deadline))
  end

  defp launch_dbservers(state, deadline) do
    Logger.info("#{state.id}: launching dbservers")

    launch_servers(state, state.mode_state.dbservers,
      health_check: true,
      timeout: Helpers.remaining_ms(deadline)
    )
  end

  defp launch_coordinators(state, deadline) do
    Logger.info("#{state.id}: launching coordinators")

    launch_servers(state, state.mode_state.coordinators,
      health_check: true,
      timeout: Helpers.remaining_ms(deadline)
    )
  end

  defp launch_servers(state, server_ids, opts) do
    health_check? = Keyword.get(opts, :health_check, false)
    timeout = Keyword.fetch!(opts, :timeout)
    count = length(server_ids)
    on_event = state.on_event

    results =
      Task.async_stream(
        server_ids,
        &launch_single_server(state.servers[&1], &1, health_check?, timeout, on_event),
        ordered: false,
        max_concurrency: count,
        timeout: timeout + @task_stream_buffer
      )
      |> Enum.to_list()

    collect_launch_results(results, state)
  end

  defp launch_single_server(server, server_id, health_check?, timeout, on_event) do
    with :ok <- ServerProcess.launch(server.server_pid),
         os_pid = ServerProcess.os_pid(server.server_pid),
         :ok <- notify_server_started(server_id, server, os_pid, on_event),
         :ok <- maybe_health_check(server, health_check?, timeout) do
      {:ok, {server_id, os_pid}}
    end
  end

  defp notify_server_started(server_id, server, os_pid, on_event) do
    Logger.info("#{server_id}: started (os_pid=#{os_pid}), endpoint=#{server.endpoint}")

    ServerLifecycle.notify_event(
      on_event,
      {:server_started, server_id, os_pid, DateTime.utc_now()}
    )

    :ok
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
    Logger.info("#{state.id}: waiting for agency consensus")

    agent_endpoints =
      Enum.map(state.mode_state.agents, fn id -> state.servers[id].endpoint end)

    Health.wait_for_agency_ready(agent_endpoints, timeout: Helpers.remaining_ms(deadline))
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

    Helpers.stop_all_health_monitors(state)

    escalated =
      Enum.flat_map(
        [
          {"coordinators", state.mode_state.coordinators},
          {"dbservers", state.mode_state.dbservers},
          {"agents", state.mode_state.agents}
        ],
        fn {label, ids} ->
          Logger.debug("#{state.id}: stopping #{label}")
          stop_servers(ids, state, Helpers.remaining_ms(deadline))
        end
      )

    Helpers.record_shutdown_escalations(state.id, escalated)

    %{state | status: :stopped, servers: Helpers.clear_server_pids(state.servers)}
  end

  defp stop_servers(server_ids, state, timeout) do
    opts = [on_event: state.on_event]

    Task.async_stream(
      server_ids,
      &Helpers.stop_server_process(state, &1, timeout, opts),
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
    Helpers.stop_all_health_monitors(state)
    ms = state.mode_state
    all_ids = ms.agents ++ ms.dbservers ++ ms.coordinators
    stop_servers(all_ids, state, state.config.shutdown_timeout)
    Logger.debug("Rollback complete for #{state.id}")

    %{
      state
      | status: :failed,
        servers: Helpers.clear_server_pids(state.servers),
        error: reason
    }
  end

  # --- Health monitoring ---

  defp start_all_health_monitors(state) do
    ms = state.mode_state
    all_ids = ms.agents ++ ms.dbservers ++ ms.coordinators
    Logger.debug("Starting health monitors for #{length(all_ids)} servers")

    Enum.reduce_while(all_ids, {:ok, state}, fn server_id, {:ok, acc} ->
      server = acc.servers[server_id]

      case Helpers.start_single_health_monitor(server_id, server.endpoint) do
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
    for id <- state.mode_state.agents,
        server = state.servers[id],
        server != nil,
        server.operational_state in [:running, nil] do
      %{id: server.id, endpoint: server.endpoint}
    end
  end

  defp put_in_mode_state(state, key, value) do
    %{state | mode_state: Map.put(state.mode_state, key, value)}
  end
end
