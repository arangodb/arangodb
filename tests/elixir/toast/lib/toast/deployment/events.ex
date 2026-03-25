defmodule Toast.Deployment.Events do
  @moduledoc "Event emission for deployment lifecycle. Single source of truth for event format."

  require Logger

  alias ToastTest.EventStore

  @spec notify(%{id: String.t()}, atom(), map()) :: :ok
  def notify(state, event, extra \\ %{}) do
    EventStore.notify(
      Map.merge(%{event: event, deployment_id: state.id, timestamp: Toast.get_timestamp()}, extra)
    )
  end

  @spec server_started(String.t(), %{endpoint: String.t()}, term(), String.t()) :: :ok
  def server_started(server_id, server, os_pid, deployment_id) do
    Logger.info("#{server_id}: started (os_pid=#{os_pid}), endpoint=#{server.endpoint}")

    EventStore.notify(%{
      event: :server_started,
      deployment_id: deployment_id,
      server_id: server_id,
      pid: os_pid,
      timestamp: Toast.get_timestamp()
    })

    :ok
  end

  @spec server_stopped(String.t(), %{pid: term()}, String.t()) :: :ok
  def server_stopped(server_id, server, deployment_id) do
    EventStore.notify(%{
      event: :server_stopped,
      deployment_id: deployment_id,
      server_id: server_id,
      pid: server.pid,
      reason: nil,
      timestamp: Toast.get_timestamp()
    })
  end
end
