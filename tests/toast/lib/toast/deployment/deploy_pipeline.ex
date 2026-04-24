defmodule Toast.Deployment.DeployPipeline do
  @moduledoc "Multi-phase deploy sequence: start processes, launch by role group, health check, post-deploy."

  require Logger

  alias Toast.Deployment.{Events, Health, ServerInstance, ServerLifecycle}
  alias Toast.Deployment.Controller.State
  alias Toast.Process.ServerProcess
  alias Toast.Process.Supervisor, as: ProcessSupervisor

  @spec run(State.t(), [map()], timeout(), keyword()) ::
          {:ok, State.t()} | {:error, term(), State.t()}
  def run(state, specs, timeout, opts) do
    Logger.debug("Starting deploy for #{state.id} (timeout=#{timeout}ms)")
    deadline = System.monotonic_time(:millisecond) + timeout
    state = init_servers_from_specs(state, specs)

    Events.notify(state.event_listener, state, :deployment_starting, %{
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

      Events.notify(state.event_listener, state, :deployment_started, %{
        servers:
          Map.new(state.servers, fn {id, s} ->
            {id, %{role: s.role, endpoint: s.endpoint, log_file: s.log_file}}
          end)
      })

      Logger.info("Deployment #{state.id} ready")
      {:ok, state}
    else
      {:error, reason} ->
        {:error, reason, state}
    end
  end

  # --- Pipeline steps ---

  defp init_servers_from_specs(state, specs) do
    servers =
      Map.new(specs, fn spec ->
        http_scheme = if spec.ssl, do: "https", else: "http"

        {spec.id,
         %ServerInstance{
           id: spec.id,
           role: spec.role,
           port: spec.port,
           endpoint: "#{http_scheme}://127.0.0.1:#{spec.port}",
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
      State.role_deploy_order(),
      {:ok, state},
      fn role, {:ok, acc} ->
        case Map.get(grouped, role) do
          nil ->
            {:cont, {:ok, acc}}

          role_specs ->
            deploy_role_group(acc, role, role_specs, deadline)
        end
      end
    )
  end

  defp deploy_role_group(state, role, role_specs, deadline) do
    server_ids = Enum.map(role_specs, & &1.id)

    with {:ok, state} <- launch_group(state, server_ids, role_opts(role), deadline),
         {:ok, state} <- post_group_hook(state, role, deadline) do
      {:cont, {:ok, state}}
    else
      err -> {:halt, err}
    end
  end

  defp role_opts(:agent), do: [health_check: false]
  defp role_opts(_role), do: [health_check: true]

  defp post_group_hook(state, :agent, deadline) do
    Logger.info("#{state.id}: waiting for agency consensus")

    endpoints_for_role(state, :agent)
    |> Health.wait_for_agency_ready(
      timeout: State.remaining_ms(deadline),
      auth: Toast.JWT.Provider.maybe_auth(state.jwt_provider)
    )
    |> case do
      :ok -> {:ok, state}
      error -> error
    end
  end

  defp post_group_hook(state, _role, _deadline), do: {:ok, state}

  defp launch_group(state, server_ids, opts, deadline) do
    health_check? = Keyword.get(opts, :health_check, false)
    timeout = State.remaining_ms(deadline)
    count = length(server_ids)
    deployment_id = state.id

    role_label = state.servers[hd(server_ids)].role
    listener = state.event_listener
    provider = state.jwt_provider

    Logger.info("#{state.id}: launching #{role_label}s")

    results =
      Task.async_stream(
        server_ids,
        &launch_server_process(
          state.servers[&1],
          &1,
          health_check?,
          timeout,
          deployment_id,
          listener,
          provider
        ),
        ordered: false,
        max_concurrency: count,
        timeout: timeout + State.task_stream_buffer()
      )
      |> Enum.to_list()

    collect_launch_results(results, state)
  end

  defp launch_server_process(
         server,
         server_id,
         health_check?,
         timeout,
         deployment_id,
         listener,
         provider
       ) do
    with :ok <- ServerProcess.launch(server.server_pid),
         os_pid = ServerProcess.os_pid(server.server_pid),
         :ok <- Events.server_started(listener, server_id, server, os_pid, deployment_id),
         :ok <- maybe_health_check(server, health_check?, timeout, provider) do
      {:ok, {server_id, os_pid}}
    end
  end

  defp maybe_health_check(_server, false, _timeout, _provider), do: :ok

  defp maybe_health_check(server, true, timeout, provider) do
    process_check_fn = fn -> ServerProcess.status(server.server_pid) == :running end

    Health.wait_until_ready(server.endpoint,
      timeout: timeout,
      process_check_fn: process_check_fn,
      auth: Toast.JWT.Provider.maybe_auth(provider)
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

      case ServerLifecycle.start_health_monitor(
             server_id,
             server.endpoint,
             jwt_provider: acc.jwt_provider
           ) do
        {:ok, pid} ->
          updated = %{server | health_monitor: pid}
          {:cont, {:ok, %{acc | servers: Map.put(acc.servers, server_id, updated)}}}

        {:error, reason} ->
          {:halt, {:error, reason}}
      end
    end)
  end

  # --- Post-deploy ---

  defp post_deploy(state) do
    case endpoints_for_role(state, :coordinator) do
      [] -> state
      [endpoint | _] -> fetch_arango_ids(state, endpoint)
    end
  end

  defp fetch_arango_ids(state, endpoint) do
    url = "#{endpoint}/_admin/cluster/health"
    req_opts = req_opts_for(state, endpoint)

    case Req.get(url, req_opts) do
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

          Events.notify(acc.event_listener, acc, :server_identified, %{
            server_id: toast_id,
            arango_id: arango_id
          })

          acc
      end
    end)
  end

  defp find_toast_id_by_endpoint(state, endpoint) do
    case extract_port(endpoint) do
      {:ok, port} -> find_toast_id_by_port(state.servers, port)
      :error -> nil
    end
  end

  defp find_toast_id_by_port(servers, port) do
    Enum.find_value(servers, fn {toast_id, server} ->
      if server.port == port, do: toast_id
    end)
  end

  defp extract_port(endpoint) do
    case URI.parse(endpoint) do
      %URI{port: port} when is_integer(port) -> {:ok, port}
      _ -> :error
    end
  end

  # --- Helpers ---

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

  defp endpoints_for_role(state, role) do
    state.servers
    |> Enum.filter(fn {_id, s} -> s.role == role end)
    |> Enum.sort_by(fn {id, _} -> id end)
    |> Enum.map(fn {_id, s} -> s.endpoint end)
  end

  defp derive_mode(state) do
    if Enum.any?(state.servers, fn {_id, s} -> ServerInstance.cluster_role?(s.role) end) do
      :cluster
    else
      :single_server
    end
  end

  defp update_server(state, server_id, updates) do
    State.update_server(state, server_id, updates)
  end

  # Minting a token per call is safe: deploy phases are bounded by
  # `startup_timeout` (default 60s), well below the default 3600s lifetime.
  defp req_opts_for(%State{jwt_provider: nil}, endpoint), do: ssl_req_opts(endpoint)

  defp req_opts_for(%State{jwt_provider: p}, endpoint),
    do: [headers: [Toast.JWT.Provider.auth_header(p)]] ++ ssl_req_opts(endpoint)

  # Self-signed test certs require disabling certificate verification.
  defp ssl_req_opts("https://" <> _),
    do: [connect_options: [transport_opts: [verify: :verify_none]]]

  defp ssl_req_opts(_endpoint), do: []
end
