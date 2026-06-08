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

defmodule ToastTest.Suite do
  @moduledoc "Behaviour for Toast test suites that define deployment configuration and lifecycle hooks."

  @callback deployment_config() :: keyword()
  @callback setup_deployment(Toast.Deployment.t()) :: {:ok, map()} | {:error, term()}
  @callback teardown_deployment(Toast.Deployment.t()) :: :ok
  @callback between_tests(Toast.Deployment.t(), ExUnit.Test.t()) :: :ok | {:error, term()}
  @optional_callbacks [setup_deployment: 1, teardown_deployment: 1, between_tests: 2]

  @default_opts [
    mode: :auto,
    timeout: 3_600_000,
    server_args: %{},
    coordinator_args: %{},
    dbserver_args: %{},
    agent_args: %{},
    between_tests: :default,
    authentication: false,
    jwt_algorithm: :hmac
  ]

  @doc "Returns the weight declared by a test module, defaulting to 1."
  @spec weight(module()) :: pos_integer()
  def weight(module) do
    if function_exported?(module, :__toast_weight__, 0),
      do: module.__toast_weight__(),
      else: 1
  end

  defmacro __using__(opts \\ []) do
    defaults = Macro.escape(@default_opts)

    # Don't use ExUnit.CaseTemplate here — its @before_compile hook
    # would override our __using__ macro definition.
    quote do
      @behaviour ToastTest.Suite

      @impl ToastTest.Suite
      def deployment_config, do: Keyword.merge(unquote(defaults), unquote(opts))

      defoverridable deployment_config: 0

      defmacro __using__(test_opts) do
        suite_module = __MODULE__
        {weight, case_opts} = Keyword.pop(test_opts, :weight, 1)

        quote do
          use ToastTest.Case, unquote(case_opts)
          @toast_suite unquote(suite_module)
          def __toast_suite__, do: unquote(suite_module)
          def __toast_weight__, do: unquote(weight)
        end
      end
    end
  end
end
