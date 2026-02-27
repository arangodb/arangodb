defmodule ToastTest.SuiteRun do
  @moduledoc false

  defstruct [
    :suite_module,
    :deployment,
    :suite_deadline,
    :timeout_factor,
    results: [],
    diagnostics: nil,
    sanitizer_matching: %{},
    crash_matching: %{}
  ]

  @type t :: %__MODULE__{
          suite_module: module(),
          deployment: Toast.Deployment.t() | nil,
          suite_deadline: integer(),
          timeout_factor: float(),
          results: [map()],
          diagnostics: map() | nil,
          sanitizer_matching: map(),
          crash_matching: map()
        }
end
