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

defmodule ToastTest.Case do
  @moduledoc """
  ExUnit.CaseTemplate for Toast test modules.

  Provides test context with `deployment`, `endpoint`, and `client` keys.
  Test modules must belong to a suite (via `use YourSuite`) which handles
  deployment lifecycle.

  ## Usage

  In `suite.ex`:

      defmodule Smoke.Suite do
        use ToastTest.Suite
      end

  In test modules:

      defmodule Smoke.VersionTest do
        use Smoke.Suite

        test "server version", %{client: client} do
          assert {:ok, %{"server" => "arango"}} = Client.Admin.version(client)
        end
      end

  ## Deployment tags

  Tests can be restricted to a specific deployment mode:

      @tag :cluster_only
      test "sharding", %{client: client} do
        # only runs with --cluster
      end

      @tag :single_only
      test "local feature", %{client: client} do
        # only runs with single server (skipped in cluster mode)
      end

  These tags also work as `@moduletag` to apply to all tests in a module.
  """

  use ExUnit.CaseTemplate

  using do
    quote do
      alias Toast.Client
      import ToastTest.Expect, only: [expect: 1]
      import ToastTest.WithDeployment, only: [with_deployment: 1, with_deployment: 2]
    end
  end

  setup_all context do
    suite_key = resolve_suite_key(context.module)

    case ToastTest.DeploymentRegistry.fetch(suite_key) do
      {:ok, deployment} -> build_deployment_context(deployment, suite_key)
      :error -> {:ok, %{}}
    end
  end

  defp build_deployment_context(deployment, suite_key) do
    extra_context = ToastTest.DeploymentRegistry.get_extra_context(suite_key)

    endpoint = Toast.Deployment.default_endpoint(deployment)

    client =
      Toast.Client.new(endpoint,
        api_version: deployment.api_version,
        protocol: deployment.protocol
      )

    client =
      case deployment.jwt_provider do
        nil -> client
        provider -> Toast.Client.with_auth(client, {:jwt_provider, provider})
      end

    base = %{
      deployment: deployment,
      endpoint: endpoint,
      client: client
    }

    Map.merge(base, extra_context)
  end

  defp resolve_suite_key(module) do
    if function_exported?(module, :__toast_suite__, 0) do
      module.__toast_suite__()
    else
      raise """
      #{inspect(module)} must belong to a suite.
      Use `use YourSuite` instead of `use ToastTest.Case` directly.
      """
    end
  end
end
