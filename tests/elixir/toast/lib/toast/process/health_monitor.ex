defmodule Toast.Process.HealthMonitor do
  @moduledoc """
  Periodically polls a server's HTTP endpoint to detect unresponsive servers.

  Complements erlexec's process exit detection (which catches crashes) by
  detecting servers that are still running but not serving requests (deadlocks,
  infinite loops, resource exhaustion).

  After `max_failures` consecutive failed health checks, notifies the listener
  with `{:server_unhealthy, server_id}` and stops polling.

  Supports `:suspend` and `:resume` messages to temporarily pause monitoring
  (e.g., during intentional server stops).
  """

  use GenServer

  require Logger

  alias Toast.Deployment.Health

  @default_interval 1_000
  @default_max_failures 3
  @http_timeout 2_000

  # --- Client API ---

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts) do
    GenServer.start_link(__MODULE__, opts)
  end

  @spec status(GenServer.server()) :: :healthy | :unhealthy | :suspended
  def status(server) do
    GenServer.call(server, :status)
  end

  @spec healthy?(GenServer.server()) :: boolean()
  def healthy?(server), do: status(server) == :healthy

  @spec suspend(GenServer.server()) :: :ok
  def suspend(server) do
    send(server, :suspend)
    :ok
  end

  @spec resume(GenServer.server()) :: :ok
  def resume(server) do
    send(server, :resume)
    :ok
  end

  @spec stop(GenServer.server()) :: :ok
  def stop(server) do
    GenServer.stop(server, :normal)
  end

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    state = %{
      server_id: Keyword.fetch!(opts, :server_id),
      endpoint: Keyword.fetch!(opts, :endpoint),
      listener: Keyword.fetch!(opts, :listener),
      interval: Keyword.get(opts, :interval, @default_interval),
      max_failures: Keyword.get(opts, :max_failures, @default_max_failures),
      consecutive_failures: 0,
      status: :healthy,
      timer_ref: nil
    }

    {:ok, schedule_check(state)}
  end

  @impl true
  def handle_call(:healthy?, _from, state) do
    {:reply, state.status == :healthy, state}
  end

  def handle_call(:status, _from, state) do
    {:reply, state.status, state}
  end

  @impl true
  def handle_info(:check, %{status: status} = state) when status in [:unhealthy, :suspended] do
    {:noreply, state}
  end

  def handle_info(:check, state) do
    case Health.check_once(state.endpoint, http_timeout: @http_timeout) do
      :ok ->
        {:noreply, schedule_check(%{state | consecutive_failures: 0})}

      {:error, reason} ->
        failures = state.consecutive_failures + 1

        if failures >= state.max_failures do
          Logger.error(
            "#{state.server_id}: health check failed #{failures} times " <>
              "(last: #{inspect(reason)}), declaring unhealthy"
          )

          send(state.listener, {:server_unhealthy, state.server_id})
          {:noreply, %{state | consecutive_failures: failures, status: :unhealthy}}
        else
          Logger.warning(
            "#{state.server_id}: health check failed (#{failures}/#{state.max_failures}, " <>
              "reason: #{inspect(reason)})"
          )

          {:noreply, schedule_check(%{state | consecutive_failures: failures})}
        end
    end
  end

  def handle_info(:suspend, state) do
    if state.timer_ref, do: Process.cancel_timer(state.timer_ref)
    {:noreply, %{state | status: :suspended, timer_ref: nil}}
  end

  def handle_info(:resume, %{status: :suspended} = state) do
    {:noreply, schedule_check(%{state | status: :healthy, consecutive_failures: 0})}
  end

  def handle_info(:resume, state) do
    {:noreply, state}
  end

  def handle_info(_msg, state) do
    {:noreply, state}
  end

  # --- Internal ---

  defp schedule_check(state) do
    ref = Process.send_after(self(), :check, state.interval)
    %{state | timer_ref: ref}
  end
end
