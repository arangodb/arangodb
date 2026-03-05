defmodule Toast.Diagnostics.Result do
  alias Toast.Diagnostics.ServerDiagnostics

  @type t :: %__MODULE__{
          servers: %{String.t() => ServerDiagnostics.t()},
          agency_dump: term() | nil,
          coredump_reports: [map()]
        }

  defstruct servers: %{},
            agency_dump: nil,
            coredump_reports: []
end
