defmodule Toast.Diagnostics.LogReport do
  @type t :: %__MODULE__{
          signal_number: non_neg_integer() | nil,
          signal_name: String.t() | nil,
          crash_header: String.t() | nil,
          backtrace: [String.t()],
          fatal_lines: [String.t()],
          crash_output: [String.t()],
          timestamp: DateTime.t() | nil,
          assertion_failures: [Toast.Diagnostics.LogAnalyzer.log_entry()],
          warnings: [Toast.Diagnostics.LogAnalyzer.log_entry()]
        }

  defstruct signal_number: nil,
            signal_name: nil,
            crash_header: nil,
            backtrace: [],
            fatal_lines: [],
            crash_output: [],
            timestamp: nil,
            assertion_failures: [],
            warnings: []
end
