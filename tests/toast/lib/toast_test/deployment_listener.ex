defmodule ToastTest.ManagedDeploymentListener do
  @moduledoc """
  Event listener for suite-managed deployments.

  Records all events in the EventStore and aborts the test run
  on unexpected server crashes.
  """
  @behaviour Toast.Deployment.EventListener

  @impl true
  def on_event(
        %{event: :server_crashed, expected: false, server_id: server_id, crash_info: crash_info} =
          event
      ) do
    ToastTest.EventStore.notify(event)
    ToastTest.CrashMonitor.handle_crash(server_id, crash_info)
  end

  @impl true
  def on_event(event), do: ToastTest.EventStore.notify(event)
end
