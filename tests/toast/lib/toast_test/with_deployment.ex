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

defmodule ToastTest.WithDeployment do
  @moduledoc """
  Helper for starting scoped deployments within individual test cases.

  Primarily used in `mode: :manual` suites where tests manage their own
  deployments, but available to any test that needs an additional deployment.

  ## Examples

      # Start a deployment using the global mode (from CLI/env) with overrides:
      with_deployment [server_args: %{"log.level" => "trace"}], fn deployment ->
        endpoint = Toast.Deployment.default_endpoint(deployment)
        assert {:ok, %{status: 200}} = Req.get(endpoint <> "/_api/version")
      end

      # Start an explicit cluster deployment:
      with_deployment [mode: :cluster, cluster_dbservers: 3], fn deployment ->
        # ...
      end

      # Start a deployment with defaults (global mode, no overrides):
      with_deployment fn deployment ->
        # ...
      end
  """

  @doc """
  Start a deployment, execute `fun`, and guarantee shutdown.

  Options use the same flat vocabulary as suite configuration:
  `:mode`, `:server_args`, `:authentication`, `:jwt_algorithm`,
  `:cluster_dbservers`, `:cluster_coordinators`, etc.

  When `:mode` is `:auto` (the default), the global deployment mode
  from CLI/environment is used.
  """
  @spec with_deployment(keyword(), (Toast.Deployment.t() -> result)) :: result when result: term()
  def with_deployment(opts \\ [], fun) do
    mode = resolve_mode(opts)
    config = ToastTest.DeployConfig.build(mode, opts)

    id = Toast.Deployment.generate_id(mode)

    base_dir =
      Application.get_env(:toast, :base_dir) ||
        raise "Toast :base_dir not configured — is Toast.Env loaded?"

    deployment_dir = Path.join([base_dir, "manual", id])

    case Toast.Deployment.start(config, deployment_dir,
           id: id,
           event_listener: ToastTest.DeploymentEventListener
         ) do
      {:ok, deployment} ->
        try do
          fun.(deployment)
        after
          Toast.Deployment.stop(deployment)
        end

      {:error, reason} ->
        raise "with_deployment failed to start: #{inspect(reason)}"
    end
  end

  defp resolve_mode(opts) do
    case Keyword.get(opts, :mode, :auto) do
      :auto ->
        Application.get_env(:toast, :deployment_mode, :single_server)

      :manual ->
        raise ArgumentError,
              "mode: :manual is not valid for with_deployment — it already is a manual deployment"

      mode ->
        mode
    end
  end
end
