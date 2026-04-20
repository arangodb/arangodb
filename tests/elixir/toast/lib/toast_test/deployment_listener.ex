defmodule ToastTest.DeploymentListener do
  @moduledoc "Event listener that delegates to EventStore and CrashMonitor."
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
