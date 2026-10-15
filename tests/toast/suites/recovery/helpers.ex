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

defmodule Recovery.Helpers do
  @moduledoc """
  Shared helpers for recovery tests.
  """

  alias Toast.Deployment

  @doc """
  Returns a properly configured client for the deployment's default server
  (single server or first coordinator), including JWT auth and protocol settings.
  """
  def default_client!(deployment) do
    servers = Deployment.servers(deployment)

    server =
      Enum.find(servers, &(&1.role in [:single, :coordinator])) ||
        raise "no single server or coordinator in deployment #{deployment.id}"

    Deployment.client!(deployment, server.id)
  end

  @doc """
  Kills all servers in the deployment and restarts them, simulating a crash
  recovery cycle. The deployment is healthy and ready when this returns.

  Uses `kill_server` (SIGKILL) followed by `start_server`, which waits for
  the server to become healthy again. We don't use `expect_crash`/`verify_crash`
  here because `kill_server` already marks the server as expecting exit (so the
  health monitor won't abort the suite), and the ServerProcess suppresses crash
  notifications for explicitly killed processes.
  """
  def crash_and_recover!(deployment) do
    servers = Deployment.servers(deployment)

    for server <- servers do
      :ok = Deployment.kill_server(deployment, server.id)
    end

    for server <- servers do
      :ok = Deployment.start_server(deployment, server.id)
    end

    :ok
  end
end
