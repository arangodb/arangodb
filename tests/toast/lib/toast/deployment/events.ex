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
  @moduledoc "Event emission for deployment lifecycle. Single source of truth for event format."

  require Logger

  alias Toast.Deployment.ServerInstance

  @spec notify(module(), %{:id => String.t(), optional(atom()) => term()}, atom(), map()) :: :ok
  def notify(listener, state, event, extra \\ %{}) do
    listener.on_event(
      Map.merge(%{event: event, deployment_id: state.id, timestamp: Toast.get_timestamp()}, extra)
    )
  end

  @spec server_started(module(), String.t(), ServerInstance.t(), term(), String.t()) :: :ok
  def server_started(listener, server_id, server, os_pid, deployment_id) do
    Logger.info("#{server_id}: started (os_pid=#{os_pid}), endpoint=#{server.endpoint}")

    listener.on_event(%{
      event: :server_started,
      deployment_id: deployment_id,
      server_id: server_id,
      pid: os_pid,
      timestamp: Toast.get_timestamp()
    })

    :ok
  end

  @spec server_stopped(module(), String.t(), ServerInstance.t(), String.t()) :: :ok
  def server_stopped(listener, server_id, server, deployment_id) do
    listener.on_event(%{
      event: :server_stopped,
      deployment_id: deployment_id,
      server_id: server_id,
      pid: server.pid,
      reason: nil,
      timestamp: Toast.get_timestamp()
    })
  end
end
