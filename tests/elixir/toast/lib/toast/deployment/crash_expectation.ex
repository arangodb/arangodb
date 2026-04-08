defmodule Toast.Deployment.CrashExpectation do
  @moduledoc false

  @enforce_keys [:timer]
  defstruct [:timer, :crash_info, :waiter]

  @type t :: %__MODULE__{
          timer: reference(),
          crash_info: Toast.Process.CrashInfo.t() | nil,
          waiter: {GenServer.from(), reference()} | nil
        }
end
