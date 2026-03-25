defmodule ToastTest.SuiteRun do
  @moduledoc false

  defstruct [
    :suite_module,
    :deployment_mode,
    :suite_deadline,
    :timeout_factor,
    :test_config,
    between_tests: :default
  ]

  @type t :: %__MODULE__{
          suite_module: module(),
          deployment_mode: :cluster | :single_server,
          suite_deadline: integer() | nil,
          timeout_factor: number(),
          test_config: ToastTest.Config.t(),
          between_tests: :default | false
        }
end
