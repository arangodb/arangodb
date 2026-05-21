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

defmodule ToastTest.ManagedDeploymentListener do
  @moduledoc """
  Event listener for suite-managed deployments.

  Records all events in the EventStore and aborts the test run
  on unexpected server crashes.
  """
  @behaviour Toast.Deployment.EventListener

  @impl true
  def on_event(
        %{event: :server_crashed, expected: false, server_id: server_id, crash_info: crash_info} =
          event
      ) do
    ToastTest.EventStore.notify(event)
    ToastTest.CrashMonitor.handle_crash(server_id, crash_info)
  end

  @impl true
  def on_event(event), do: ToastTest.EventStore.notify(event)
end
