################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule Toast.Deployment.HealthBarrier do
  @moduledoc """
  Between-tests barrier that ensures each server's HealthMonitor reports a
  healthy probe before the test runner advances.

  Complements the `CrashBarrier` (which catches in-flight crashes via
  `/proc/<pid>/status`) by catching "alive but unresponsive" cases —
  deadlocks, stuck I/O, resource exhaustion — that the kernel view can't see.

  Liveness and availability are distinct properties; this module handles
  availability only.

  > #### Not safe inside a GenServer callback {: .warning}
  > `await_healthy/2` uses `Process.sleep/1` in its poll loop and may block
  > for up to the configured timeout. Call it only from plain processes (the
  > test runner).
  """

  require Logger

  alias Toast.Deployment
  alias Toast.Deployment.ServerInstance
  alias Toast.Deployment.ServerLifecycle
  alias Toast.Process.HealthMonitor
  alias Toast.Utils.Polling

  @type hm_probe_result :: HealthMonitor.probe_state() | :not_monitored

  @type option ::
          {:hm_probe, (ServerInstance.t() -> hm_probe_result())}
          | {:timeout, timeout()}
          | {:poll_interval, pos_integer()}

  @type result :: :ok | {:error, String.t()}

  @default_timeout 180_000
  # Slightly above HealthMonitor's default 1000ms poll interval so each tick
  # is likely to see a fresh state rather than the same one twice.
  @default_poll_interval 1_100

  @available [:healthy, :not_monitored]

  @spec await_healthy(Deployment.t(), [option()]) :: result()
  def await_healthy(%Deployment{} = deployment, opts \\ []) do
    case Deployment.status(deployment) do
      :failed ->
        :ok

      _ ->
        deployment
        |> Deployment.server_instances()
        |> Enum.reduce_while(:ok, fn server, :ok ->
          check_server(server, opts)
        end)
    end
  end

  defp check_server(%ServerInstance{operational_state: :running} = server, opts) do
    hm_probe = Keyword.get(opts, :hm_probe, &ServerLifecycle.probe_health_monitor/1)

    case hm_probe.(server) do
      state when state in @available -> {:cont, :ok}
      :suspended -> {:halt, fail_suspended(server)}
      :unhealthy -> {:halt, fail_unhealthy(server)}
      :failing -> wait_for_recovery(server, hm_probe, opts)
    end
  end

  defp check_server(_server, _opts), do: {:cont, :ok}

  defp wait_for_recovery(server, hm_probe, opts) do
    timeout = Keyword.get(opts, :timeout, @default_timeout)
    poll_interval = Keyword.get(opts, :poll_interval, @default_poll_interval)
    deadline = System.monotonic_time(:millisecond) + timeout

    Logger.warning(
      "#{server.id} health monitor reports failing probes, " <>
        "blocking up to #{timeout}ms for recovery or declared failure"
    )

    # HM can be suspended concurrently via ServerLifecycle.suspend_health_monitor
    # (stop/pause/expect_crash paths), so :suspended is reachable mid-poll.
    case Polling.poll_until(fn -> classify_tick(hm_probe.(server)) end, deadline, poll_interval) do
      {:ok, :recovered} ->
        Logger.info("#{server.id} health monitor recovered, proceeding")
        {:cont, :ok}

      {:ok, :unhealthy} ->
        {:halt, fail_unhealthy(server)}

      {:ok, :suspended} ->
        {:halt, fail_suspended(server)}

      {:error, :timeout} ->
        message =
          "Server #{server.id} health monitor did not recover within #{timeout}ms"

        Logger.error(message)
        {:halt, {:error, message}}
    end
  end

  defp fail_suspended(server) do
    message =
      "Server #{server.id} health monitor is suspended between tests — " <>
        "a prior test failed to resume it (expect_crash without restore?)"

    Logger.error(message)
    {:error, message}
  end

  defp fail_unhealthy(server) do
    {:error, "Server #{server.id} health monitor declared it unresponsive"}
  end

  defp classify_tick(state) when state in @available, do: {:done, :recovered}
  defp classify_tick(:failing), do: :not_ready
  defp classify_tick(:unhealthy), do: {:done, :unhealthy}
  defp classify_tick(:suspended), do: {:done, :suspended}
end
