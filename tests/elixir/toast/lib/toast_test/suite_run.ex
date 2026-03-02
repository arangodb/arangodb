defmodule ToastTest.SuiteRun do
  @moduledoc false

  defstruct [
    :suite_module,
    :deployment,
    :suite_deadline,
    :timeout_factor
  ]

  @type t :: %__MODULE__{
          suite_module: module(),
          deployment: Toast.Deployment.t() | nil,
          suite_deadline: integer(),
          timeout_factor: float()
        }
end
