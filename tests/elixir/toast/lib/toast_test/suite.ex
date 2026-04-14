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

        quote do
          use ToastTest.Case, unquote(test_opts)
          @toast_suite unquote(suite_module)
          def __toast_suite__, do: unquote(suite_module)
        end
      end
    end
  end
end
