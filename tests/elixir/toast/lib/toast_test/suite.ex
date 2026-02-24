defmodule ToastTest.Suite do
  @callback deployment_config() :: keyword()
  @callback setup_deployment(map()) :: {:ok, map()} | {:error, term()}
  @callback teardown_deployment(map()) :: :ok
  @callback between_tests(map(), ExUnit.Test.t()) :: :ok | {:error, term()}
  @optional_callbacks [setup_deployment: 1, teardown_deployment: 1, between_tests: 2]

  @default_opts [
    mode: :auto,
    timeout: 3_600_000,
    server_args: [],
    coordinator_args: [],
    dbserver_args: [],
    agent_args: [],
    between_tests: :default
  ]

  defmacro __using__(opts \\ []) do
    merged = Keyword.merge(@default_opts, opts)

    # Don't use ExUnit.CaseTemplate here — its @before_compile hook
    # would override our __using__ macro definition.
    quote do
      @behaviour ToastTest.Suite

      @impl ToastTest.Suite
      def deployment_config, do: unquote(merged)

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
