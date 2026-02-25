defmodule Toast.Deployment.ServerInstance do
  @moduledoc "Runtime state of a server instance in a deployment."

  @type role :: :single | :agent | :dbserver | :coordinator
  @type operational_state :: :running | :paused | :stopped | :killed | :crashed

  @type t :: %__MODULE__{
          id: String.t(),
          role: role(),
          port: non_neg_integer() | nil,
          endpoint: String.t() | nil,
          pid: non_neg_integer() | nil,
          log_file: Path.t() | nil,
          server_dir: Path.t() | nil,
          server_pid: pid() | nil,
          health_monitor: pid() | nil,
          operational_state: operational_state() | nil,
          intentional: boolean(),
          launch_spec: map() | nil
        }

  @enforce_keys [:id, :role]
  defstruct [:id, :role, :port, :endpoint, :pid, :log_file, :server_dir,
             :server_pid, :health_monitor, :operational_state, :launch_spec,
             intentional: false]
end
