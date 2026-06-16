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

defmodule Toast.Deployment.Events do
  @moduledoc """
  Event emission for the deployment lifecycle.

  Every deployment event has exactly one constructor here; no caller builds
  event maps by hand. The full event vocabulary (fields, producers, consumers)
  is documented in `docs/events.md`.

  Payloads are plain maps, never producer structs: events are persisted (ETF)
  and replayed by later `mix toast.analyze` runs, so they must carry no runtime
  state (PIDs) and not pin struct shapes across versions.
  """

  require Logger

  alias Toast.Deployment.ServerInstance
  alias Toast.Process.CrashInfo

  @type listener :: module()
  @type deployment_id :: String.t()

  @typedoc "Affected-server entry carried by `:timeout_kill` events."
  @type timeout_kill_server :: %{
          server_id: String.t(),
          os_pid: non_neg_integer() | nil,
          log_file: Path.t() | nil
        }

  @spec deployment_starting(
          listener(),
          deployment_id(),
          atom(),
          list() | nil,
          %{String.t() => ServerInstance.t()}
        ) :: :ok
  def deployment_starting(listener, deployment_id, mode, stacktrace, servers)
      when is_atom(listener) and is_binary(deployment_id) and is_atom(mode) and
             (is_list(stacktrace) or is_nil(stacktrace)) and is_map(servers) do
    specs =
      servers
      |> Map.values()
      |> Enum.map(&Map.take(&1, [:id, :role, :port, :endpoint, :log_file, :server_dir]))

    payload = %{mode: mode, stacktrace: stacktrace, specs: specs}
    emit(listener, deployment_id, :deployment_starting, payload)
  end

  @spec deployment_started(listener(), deployment_id()) :: :ok
  def deployment_started(listener, deployment_id)
      when is_atom(listener) and is_binary(deployment_id) do
    emit(listener, deployment_id, :deployment_started, %{})
  end

  @spec deployment_stopped(listener(), deployment_id()) :: :ok
  def deployment_stopped(listener, deployment_id)
      when is_atom(listener) and is_binary(deployment_id) do
    emit(listener, deployment_id, :deployment_stopped, %{})
  end

  @spec server_identified(listener(), deployment_id(), String.t(), String.t()) :: :ok
  def server_identified(listener, deployment_id, server_id, arango_id)
      when is_atom(listener) and is_binary(deployment_id) and is_binary(server_id) and
             is_binary(arango_id) do
    payload = %{server_id: server_id, arango_id: arango_id}
    emit(listener, deployment_id, :server_identified, payload)
  end

  @spec server_started(
          listener(),
          deployment_id(),
          String.t(),
          ServerInstance.t(),
          non_neg_integer() | nil
        ) :: :ok
  def server_started(listener, deployment_id, server_id, %ServerInstance{} = server, os_pid)
      when is_atom(listener) and is_binary(deployment_id) and is_binary(server_id) and
             (is_integer(os_pid) or is_nil(os_pid)) do
    Logger.info("#{server_id}: started (os_pid=#{os_pid}), endpoint=#{server.endpoint}")
    payload = %{server_id: server_id, pid: os_pid}
    emit(listener, deployment_id, :server_started, payload)
  end

  @spec server_stopped(listener(), deployment_id(), String.t(), non_neg_integer() | nil) :: :ok
  def server_stopped(listener, deployment_id, server_id, os_pid)
      when is_atom(listener) and is_binary(deployment_id) and is_binary(server_id) and
             (is_integer(os_pid) or is_nil(os_pid)) do
    payload = %{server_id: server_id, pid: os_pid, reason: nil}
    emit(listener, deployment_id, :server_stopped, payload)
  end

  @spec server_killed(listener(), deployment_id(), String.t(), non_neg_integer() | nil) :: :ok
  def server_killed(listener, deployment_id, server_id, os_pid)
      when is_atom(listener) and is_binary(deployment_id) and is_binary(server_id) and
             (is_integer(os_pid) or is_nil(os_pid)) do
    emit(listener, deployment_id, :server_killed, %{server_id: server_id, pid: os_pid})
  end

  @spec server_paused(listener(), deployment_id(), String.t()) :: :ok
  def server_paused(listener, deployment_id, server_id)
      when is_atom(listener) and is_binary(deployment_id) and is_binary(server_id) do
    emit(listener, deployment_id, :server_paused, %{server_id: server_id})
  end

  @spec server_resumed(listener(), deployment_id(), String.t()) :: :ok
  def server_resumed(listener, deployment_id, server_id)
      when is_atom(listener) and is_binary(deployment_id) and is_binary(server_id) do
    emit(listener, deployment_id, :server_resumed, %{server_id: server_id})
  end

  @spec server_unhealthy(listener(), deployment_id(), String.t()) :: :ok
  def server_unhealthy(listener, deployment_id, server_id)
      when is_atom(listener) and is_binary(deployment_id) and is_binary(server_id) do
    emit(listener, deployment_id, :server_unhealthy, %{server_id: server_id})
  end

  @spec server_crashed(listener(), deployment_id(), String.t(), CrashInfo.t(), boolean()) :: :ok
  def server_crashed(listener, deployment_id, server_id, %CrashInfo{} = crash_info, expected)
      when is_atom(listener) and is_binary(deployment_id) and is_binary(server_id) and
             is_boolean(expected) do
    emit(listener, deployment_id, :server_crashed, %{
      server_id: server_id,
      pid: crash_info.os_pid,
      crash_info: crash_info,
      expected: expected
    })
  end

  @spec timeout_kill(listener(), deployment_id(), atom(), String.t(), [timeout_kill_server()]) ::
          :ok
  def timeout_kill(listener, deployment_id, source, reason, servers)
      when is_atom(listener) and is_binary(deployment_id) and is_atom(source) and
             is_binary(reason) and is_list(servers) do
    payload = %{source: source, reason: reason, servers: servers}
    emit(listener, deployment_id, :timeout_kill, payload)
  end

  defp emit(listener, deployment_id, event, payload) do
    %{event: event, deployment_id: deployment_id, timestamp: Toast.get_timestamp()}
    |> Map.merge(payload)
    |> listener.on_event()

    :ok
  end
end
