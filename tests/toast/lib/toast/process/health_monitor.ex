defmodule Toast.Process.HealthMonitor do
  @moduledoc """
  Periodically polls a server's HTTP endpoint to detect unresponsive servers.

  Complements erlexec's process exit detection (which catches crashes) by
  detecting servers that are still running but not serving requests (deadlocks,
  infinite loops, resource exhaustion).

  After `max_failures` consecutive failed health checks, notifies the listener
  with `{:server_unhealthy, server_id}` and stops polling.

  Supports `suspend/1` and `resume/1` to temporarily pause monitoring
  (e.g., during intentional server stops).
  """

  use GenServer, restart: :temporary

  require Logger

  alias Toast.Deployment.Health

  @default_interval 1_000
  @default_max_failures 3
  @http_timeout 2_000

  defmodule State do
    @moduledoc false
    @enforce_keys [:server_id, :endpoint, :listener, :interval, :max_failures]
    defstruct [
      :server_id,
      :endpoint,
      :listener,
      :interval,
      :max_failures,
      :timer_ref,
      :jwt_provider,
      consecutive_failures: 0,
      status: :healthy
    ]
  end

  # --- Client API ---

  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts) do
    GenServer.start_link(__MODULE__, opts)
  end

  @spec suspend(GenServer.server()) :: :ok
  def suspend(server), do: GenServer.cast(server, :suspend)

  @spec resume(GenServer.server()) :: :ok
  def resume(server), do: GenServer.cast(server, :resume)

  @doc false
  def status(server), do: GenServer.call(server, :status)

  @typedoc """
  Probe classification derived from status + consecutive_failures.

  - `:healthy` — last poll succeeded, no failures pending
  - `:failing` — at least one consecutive failure, but below max
  - `:unhealthy` — max_failures reached, monitor declared the server dead
  - `:suspended` — monitoring paused via `suspend/1`
  """
  @type probe_state :: :healthy | :failing | :unhealthy | :suspended

  @spec probe_state(GenServer.server()) :: probe_state()
  def probe_state(server), do: GenServer.call(server, :probe_state)

  @spec stop(GenServer.server()) :: :ok
  def stop(server) do
    GenServer.stop(server)
  end

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    server_id = Keyword.fetch!(opts, :server_id)
    interval = Keyword.get(opts, :interval, @default_interval)
    Logger.debug("#{server_id}: health monitor started (interval=#{interval}ms)")

    state = %State{
      server_id: server_id,
      endpoint: Keyword.fetch!(opts, :endpoint),
      listener: Keyword.fetch!(opts, :listener),
      interval: interval,
      max_failures: Keyword.get(opts, :max_failures, @default_max_failures),
      jwt_provider: Keyword.get(opts, :jwt_provider)
    }

    {:ok, schedule_check(state)}
  end

  @impl true
  def handle_call(:status, _from, state) do
    {:reply, state.status, state}
  end

  @impl true
  def handle_call(:probe_state, _from, state) do
    {:reply, derive_probe_state(state), state}
  end

  @impl true
  def handle_info(:check, %{status: status} = state) when status in [:unhealthy, :suspended] do
    {:noreply, state}
  end

  @impl true
  def handle_info(:check, state) do
    # Mint per check — minting cost is negligible next to the HTTP round-trip
    # and guarantees we never hold a stale token across long-running tests.
    auth = Toast.JWT.Provider.maybe_auth(state.jwt_provider)

    case Health.check_once(state.endpoint, http_timeout: @http_timeout, auth: auth) do
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

  def handle_info(_msg, state) do
    {:noreply, state}
  end

  @impl true
  def handle_cast(:suspend, state) do
    Logger.debug("#{state.server_id}: health monitor suspended")
    cancel_timer(state.timer_ref)
    {:noreply, %{state | status: :suspended, timer_ref: nil}}
  end

  def handle_cast(:resume, %{status: status} = state) when status in [:suspended, :unhealthy] do
    Logger.debug("#{state.server_id}: health monitor resumed")
    {:noreply, schedule_check(%{state | status: :healthy, consecutive_failures: 0})}
  end

  def handle_cast(:resume, state) do
    {:noreply, state}
  end

  # --- Internal ---

  defp derive_probe_state(%{status: :unhealthy}), do: :unhealthy
  defp derive_probe_state(%{status: :suspended}), do: :suspended
  defp derive_probe_state(%{status: :healthy, consecutive_failures: 0}), do: :healthy
  defp derive_probe_state(%{status: :healthy}), do: :failing

  defp schedule_check(state) do
    ref = Process.send_after(self(), :check, state.interval)
    %{state | timer_ref: ref}
  end

  defp cancel_timer(nil), do: :ok
  defp cancel_timer(ref), do: Process.cancel_timer(ref)
end
