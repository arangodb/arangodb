defmodule Toast.Diagnostics.Coredump.Report do
  @moduledoc "Structured report from coredump analysis."

  @type t :: %__MODULE__{
          core_path: Path.t(),
          binary_path: Path.t(),
          debugger: :gdb | :lldb,
          signal: String.t() | nil,
          faulting_address: String.t() | nil,
          registers: String.t() | nil,
          disassembly: String.t() | nil,
          threads: [map()],
          crash_thread: integer() | nil
        }

  defstruct [
    :core_path,
    :binary_path,
    :debugger,
    :signal,
    :faulting_address,
    :registers,
    :disassembly,
    :crash_thread,
    threads: []
  ]
end
