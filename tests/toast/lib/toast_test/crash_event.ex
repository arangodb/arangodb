defmodule ToastTest.CrashEvent do
  @moduledoc "Structured crash event emitted via on_event callback."

  @enforce_keys [:server_id, :crash_info]
  defstruct [:server_id, :crash_info, expected: false]

  @type t :: %__MODULE__{
          server_id: String.t(),
          crash_info: Toast.Process.CrashInfo.t(),
          expected: boolean()
        }
end
