defmodule Toast.Deployment.Controller.Helpers do
  @moduledoc false

  alias Toast.Deployment.{ServerInstance, ServerLifecycle}
  alias Toast.Process.ServerProcess
  alias Toast.Process.Supervisor, as: ProcessSupervisor

  @spec fetch_server(Toast.Deployment.Controller.State.t(), String.t()) ::
          {:ok, ServerInstance.t()} | {:error, :not_found}
  def fetch_server(state, server_id) do
    with :error <- Map.fetch(state.servers, server_id), do: {:error, :not_found}
  end

  @spec update_server(Toast.Deployment.Controller.State.t(), String.t(), keyword()) ::
          Toast.Deployment.Controller.State.t()
  def update_server(state, server_id, updates) do
    %{state | servers: Map.update!(state.servers, server_id, &struct!(&1, updates))}
  end

  @spec start_single_health_monitor(String.t(), String.t()) :: {:ok, pid()} | {:error, term()}
  def start_single_health_monitor(server_id, endpoint) do
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

  @spec stop_all_health_monitors(Toast.Deployment.Controller.State.t()) :: :ok
  def stop_all_health_monitors(state) do
    for {_id, server} <- state.servers do
      ServerLifecycle.stop_health_monitor(server)
    end

    :ok
  end

  @spec stop_health_monitor(Toast.Deployment.Controller.State.t(), String.t()) :: :ok
  def stop_health_monitor(state, server_id) do
    case state.servers[server_id] do
      nil -> :ok
      server -> ServerLifecycle.stop_health_monitor(server)
    end
  end

  @spec stop_server_process(Toast.Deployment.Controller.State.t(), String.t(), timeout()) :: :ok
  def stop_server_process(state, server_id, timeout) do
    case state.servers[server_id] do
      %{server_pid: nil} ->
        :ok

      %{server_pid: pid} ->
        try do
          ServerProcess.stop(pid, timeout)
          DynamicSupervisor.terminate_child(ProcessSupervisor, pid)
        catch
          :exit, _ -> :ok
        end

      nil ->
        :ok
    end
  end

  @spec spec_to_server_opts(map()) :: keyword()
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

  @spec clear_server_pids(map()) :: map()
  def clear_server_pids(servers) do
    Map.new(servers, fn {id, server} -> {id, %{server | server_pid: nil, health_monitor: nil}} end)
  end

  @spec remaining_ms(integer()) :: non_neg_integer()
  def remaining_ms(deadline) do
    max(0, deadline - System.monotonic_time(:millisecond))
  end

  @spec resolve_target_by_id(Toast.Deployment.Controller.State.t(), String.t()) ::
          {:ok, [String.t()]} | {:error, :not_found}
  def resolve_target_by_id(state, server_id) do
    if Map.has_key?(state.servers, server_id),
      do: {:ok, [server_id]},
      else: {:error, :not_found}
  end
end
