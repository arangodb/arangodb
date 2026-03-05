defmodule Toast.Diagnostics.Result do
  @type t :: %__MODULE__{
          servers: %{String.t() => map()},
          agency_dump: term() | nil,
          coredump_reports: [map()]
        }

  defstruct servers: %{},
            agency_dump: nil,
            coredump_reports: []
end
