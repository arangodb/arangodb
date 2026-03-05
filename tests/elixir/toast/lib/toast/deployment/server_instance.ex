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
          expecting_exit: boolean(),
          launch_spec: Toast.Deployment.Factory.LaunchSpec.t() | nil
        }

  @enforce_keys [:id, :role]
  defstruct [
    :id,
    :role,
    :port,
    :endpoint,
    :pid,
    :log_file,
    :server_dir,
    :server_pid,
    :health_monitor,
    :operational_state,
    :launch_spec,
    expecting_exit: false
  ]

  @doc "Whether this server has crashed unexpectedly (not as part of an expected exit)."
  @spec unexpected_crash?(t()) :: boolean()
  def unexpected_crash?(%__MODULE__{operational_state: :crashed, expecting_exit: false}), do: true
  def unexpected_crash?(%__MODULE__{}), do: false
end
