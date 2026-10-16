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

defmodule Toast.Deployment.ServerLifecycle do
  @moduledoc "Shared server lifecycle operations used by the deployment Controller."

  require Logger

  alias Toast.Deployment.{Health, ServerInstance}
  alias Toast.Process.{HealthMonitor, ServerProcess}
  alias Toast.Process.Supervisor, as: ProcessSupervisor

  @base_stop_timeout 30_000
  @base_relaunch_timeout 60_000
  @base_sleep_after_kill 200

  # --- Server control operations ---

  @spec stop_server(ServerInstance.t(), keyword()) :: :ok
  def stop_server(%ServerInstance{} = server, opts \\ []) do
    factor = Keyword.get(opts, :timeout_factor, 1)
    Logger.info("Stopping server #{server.id} (timeout=#{@base_stop_timeout * factor}ms)")
    ServerProcess.stop(server.server_pid, @base_stop_timeout * factor)
    suspend_health_monitor(server)
    :ok
  end

  @spec kill_server(ServerInstance.t()) :: :ok
  def kill_server(%ServerInstance{} = server) do
    Logger.info("Killing server #{server.id}")
    ServerProcess.kill(server.server_pid)
    suspend_health_monitor(server)
    :ok
  end

  @spec abort_server(ServerInstance.t()) :: :ok | {:error, :not_running}
  def abort_server(%ServerInstance{} = server) do
    Logger.info("Aborting server #{server.id} (SIGABRT)")
    suspend_health_monitor(server)
    ServerProcess.send_signal(server.server_pid, :sigabrt)
  end

  @spec pause_server(ServerInstance.t()) :: :ok
  def pause_server(%ServerInstance{} = server) do
    Logger.info("Pausing server #{server.id}")
    ServerProcess.pause(server.server_pid)
    suspend_health_monitor(server)
    :ok
  end

  @spec resume_server(ServerInstance.t()) :: :ok
  def resume_server(%ServerInstance{} = server) do
    Logger.info("Resuming server #{server.id}")
    ServerProcess.resume(server.server_pid)
    resume_health_monitor(server)
    :ok
  end

  @spec stop_before_restart(ServerInstance.t(), keyword()) :: :ok
  def stop_before_restart(%ServerInstance{} = server, opts \\ []) do
    factor = Keyword.get(opts, :timeout_factor, 1)

    Logger.info("Stopping server #{server.id} before restart (state=#{server.operational_state})")

    case server.operational_state do
      :running ->
        ServerProcess.stop(server.server_pid, @base_stop_timeout * factor)
        suspend_health_monitor(server)

      :paused ->
        ServerProcess.kill(server.server_pid)
        Process.sleep(@base_sleep_after_kill * factor)
        suspend_health_monitor(server)

      _stopped_or_crashed ->
        :ok
    end
  end

  @spec relaunch_and_wait(ServerInstance.t(), keyword()) :: :ok | {:error, term()}
  def relaunch_and_wait(%ServerInstance{} = server, opts) do
    Logger.info("Relaunching server #{server.id}")
    factor = Keyword.get(opts, :timeout_factor, 1)
    auth = Toast.JWT.Provider.maybe_auth(Keyword.get(opts, :jwt_provider))
    process_check_fn = fn -> ServerProcess.status(server.server_pid) == :running end

    with :ok <- ServerProcess.relaunch(server.server_pid, opts),
         :ok <-
           Health.wait_until_ready(server.endpoint,
             timeout: @base_relaunch_timeout * factor,
             process_check_fn: process_check_fn,
             auth: auth
           ) do
      resume_health_monitor(server)
      Logger.info("Server #{server.id} relaunched and ready")
      :ok
    end
  end

  # --- State validation ---

  @spec require_state(ServerInstance.t(), atom()) :: :ok | {:error, {:unexpected_state, atom()}}
  def require_state(%ServerInstance{} = server, expected) do
    if server.operational_state == expected,
      do: :ok,
      else: {:error, {:unexpected_state, server.operational_state}}
  end

  @spec require_state_in(ServerInstance.t(), [atom()]) ::
          :ok | {:error, {:unexpected_state, atom()}}
  def require_state_in(%ServerInstance{} = server, expected_list) do
    if server.operational_state in expected_list,
      do: :ok,
      else: {:error, {:unexpected_state, server.operational_state}}
  end

  # --- Health monitor helpers ---

  @spec start_health_monitor(String.t(), String.t(), keyword()) ::
          {:ok, pid()} | {:error, term()}
  def start_health_monitor(server_id, endpoint, opts \\ []) do
    case ProcessSupervisor.start_health_monitor(
           [server_id: server_id, endpoint: endpoint, listener: self()] ++ opts
         ) do
      {:ok, pid} ->
        Process.monitor(pid)
        {:ok, pid}

      error ->
        error
    end
  end

  @spec suspend_health_monitor(ServerInstance.t()) :: :ok
  def suspend_health_monitor(%{health_monitor: nil}), do: :ok

  def suspend_health_monitor(%{health_monitor: pid}),
    do: HealthMonitor.suspend(pid)

  @spec resume_health_monitor(ServerInstance.t() | nil) :: :ok
  def resume_health_monitor(nil), do: :ok
  def resume_health_monitor(%{health_monitor: nil}), do: :ok

  def resume_health_monitor(%{health_monitor: pid}),
    do: HealthMonitor.resume(pid)

  @spec stop_health_monitor(ServerInstance.t()) :: :ok
  def stop_health_monitor(%{health_monitor: nil}), do: :ok

  def stop_health_monitor(%{health_monitor: pid}) do
    HealthMonitor.stop(pid)
  catch
    :exit, _ -> :ok
  end

  @spec probe_health_monitor(ServerInstance.t() | nil) ::
          HealthMonitor.probe_state() | :not_monitored
  def probe_health_monitor(nil), do: :not_monitored
  def probe_health_monitor(%{health_monitor: nil}), do: :not_monitored
  def probe_health_monitor(%{health_monitor: pid}), do: HealthMonitor.probe_state(pid)

  # --- Server output ---

  @spec print_server_output(String.t(), String.t()) :: :ok
  def print_server_output(server_id, data) do
    data
    |> String.split("\n", trim: true)
    |> Enum.each(&IO.puts("  #{server_id} | #{&1}"))
  end
end
