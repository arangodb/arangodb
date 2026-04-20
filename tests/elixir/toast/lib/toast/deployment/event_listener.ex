defmodule Toast.Deployment.EventListener do
  @moduledoc "Behaviour for receiving deployment lifecycle events."

  @callback on_event(event :: map()) :: :ok
end
