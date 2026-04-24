defmodule ToastTest.DeploymentEventListener do
  @moduledoc """
  Event listener for per-test deployments.

  Records events in the EventStore but does not abort on crashes —
  the test is responsible for its own failure handling.
  """
  @behaviour Toast.Deployment.EventListener

  @impl true
  def on_event(event), do: ToastTest.EventStore.notify(event)
end
