defmodule ToastTest.SuiteRun do
  @moduledoc false

  defstruct [
    :suite_module,
    :deployment_mode,
    :suite_deadline,
    :suite_timeout,
    :global_deadline,
    :global_timeout,
    :timeout_factor,
    :test_config,
    between_tests: :default
  ]

  @type t :: %__MODULE__{
          suite_module: module(),
          deployment_mode: :cluster | :single_server,
          suite_deadline: integer() | nil,
          suite_timeout: pos_integer() | nil,
          global_deadline: integer() | nil,
          global_timeout: pos_integer() | nil,
          timeout_factor: number(),
          test_config: ToastTest.Config.t(),
          between_tests: :default | false
        }
end
