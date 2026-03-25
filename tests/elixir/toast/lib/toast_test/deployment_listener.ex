defmodule ToastTest.DeploymentListener do
  @moduledoc "Event listener that delegates to EventStore and CrashMonitor."
  @behaviour Toast.Deployment.EventListener

  @impl true
  def on_event(event), do: ToastTest.EventStore.notify(event)

  @impl true
  def on_crash(server_id, crash_info),
    do: ToastTest.CrashMonitor.handle_crash(server_id, crash_info)

  @impl true
  def on_timeout_kill(source, reason, servers),
    do: ToastTest.EventStore.record_timeout_kill(source, reason, servers)
end
