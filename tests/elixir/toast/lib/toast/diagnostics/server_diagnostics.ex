defmodule Toast.Diagnostics.ServerDiagnostics do
  alias Toast.Deployment.ServerInstance
  alias Toast.Diagnostics.{LogAnalyzer, Sanitizer}

  @enforce_keys [:server]

  @type t :: %__MODULE__{
          sanitizer_errors: [Sanitizer.sanitizer_error()],
          log_report: LogAnalyzer.log_report() | nil,
          server_error: term(),
          server: ServerInstance.t()
        }

  defstruct sanitizer_errors: [],
            log_report: nil,
            server_error: nil,
            server: nil
end
