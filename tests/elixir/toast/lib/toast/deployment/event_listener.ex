defmodule Toast.Deployment.EventListener do
  @moduledoc "Behaviour for receiving deployment lifecycle events."

  @callback on_event(event :: map()) :: :ok
  @callback on_crash(server_id :: String.t(), crash_info :: Toast.Process.CrashInfo.t()) :: :ok
  @callback on_timeout_kill(source :: atom(), reason :: String.t(), servers :: [map()]) :: :ok
end
