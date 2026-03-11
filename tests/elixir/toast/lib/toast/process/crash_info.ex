defmodule Toast.Process.CrashInfo do
  @moduledoc false

  @enforce_keys [:exit_status, :signal, :timestamp]
  defstruct [:exit_status, :signal, :timestamp]

  @type t :: %__MODULE__{
          exit_status: non_neg_integer() | nil,
          signal: non_neg_integer() | nil,
          timestamp: DateTime.t()
        }
end
