defmodule ToastTest.SuiteRun do
  @moduledoc false

  defstruct [
    :suite_module,
    :deployment_mode,
    :suite_deadline,
    :timeout_factor,
    :toast_config,
    between_tests: :default
  ]

  @type t :: %__MODULE__{
          suite_module: module(),
          deployment_mode: :cluster | :single_server,
          suite_deadline: integer() | nil,
          timeout_factor: number(),
          toast_config: Toast.Config.t() | nil,
          between_tests: :default | false
        }
end
