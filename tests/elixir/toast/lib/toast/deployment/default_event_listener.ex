defmodule Toast.Deployment.DefaultEventListener do
  @moduledoc "No-op event listener. Used when no listener is configured."
  @behaviour Toast.Deployment.EventListener

  @impl true
  def on_event(_event), do: :ok

  @impl true
  def on_crash(_server_id, _crash_info), do: :ok

  @impl true
  def on_timeout_kill(_source, _reason, _servers), do: :ok
end
