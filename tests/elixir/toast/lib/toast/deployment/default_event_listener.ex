defmodule Toast.Deployment.DefaultEventListener do
  @moduledoc "No-op event listener. Used when no listener is configured."
  @behaviour Toast.Deployment.EventListener

  @impl true
  def on_event(_event), do: :ok
end
