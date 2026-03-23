defmodule Toast.Process.CrashInfo do
  @moduledoc false

  @enforce_keys [:exit_status, :signal, :timestamp]
  defstruct [:exit_status, :signal, :timestamp, :os_pid]

  @type t :: %__MODULE__{
          exit_status: non_neg_integer() | nil,
          signal: non_neg_integer() | nil,
          timestamp: Toast.timestamp(),
          os_pid: pos_integer() | nil
        }
end
