defmodule ToastTest.SuiteRun do
  @moduledoc false

  defstruct [
    :suite_module,
    :deployment,
    :deployment_mode,
    :suite_deadline,
    :timeout_factor
  ]

  @type t :: %__MODULE__{
          suite_module: module(),
          deployment: Toast.Deployment.t() | nil,
          deployment_mode: :cluster | :single_server,
          suite_deadline: integer(),
          timeout_factor: number()
        }
end
