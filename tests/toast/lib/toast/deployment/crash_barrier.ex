defmodule Toast.Deployment.CrashBarrier do
  @moduledoc """
  Between-tests barrier that waits for an in-flight coredump to finish being
  written before the test runner advances.

  When a server crashes, the listening socket closes quickly — the test's
  pending HTTP request fails, and the test finishes. The kernel, meanwhile,
  keeps the task alive until it has finished writing the coredump, so
  erlexec's `:DOWN` → controller `:server_crashed` transition is delayed.
  Without this barrier, the test runner would happily start the next test
  (or finalize the suite) against a server that is already gone, and the
  crash would be missed entirely if the run finishes before `:DOWN` fires.

  `await_settled/2` checks the controller's status first, then inspects each
  running server via `/proc/<pid>/status`. If any server shows `CoreDumping`,
  is a zombie, or has a missing proc entry, the barrier blocks on the
  controller until the crash event has been recorded.

  This module handles liveness only. Availability (HTTP probes) is the
  `HealthBarrier`'s concern — run both at each test boundary.

  > #### Not safe inside a GenServer callback {: .warning}
  > `await_settled/2` blocks on `Controller.await_crash_event/3` for up to
  > the configured timeout. Call it only from plain processes (the test
  > runner).
  """

  require Logger

  alias Toast.Deployment
  alias Toast.Deployment.Controller
  alias Toast.Deployment.ServerInstance
  alias Toast.Process.ProcStatus

  @type option ::
          {:inspector, (pos_integer() -> ProcStatus.result())}
          | {:timeout, timeout()}

  @type result :: :ok | {:error, String.t()}

  @default_timeout 180_000

  @spec await_settled(Deployment.t(), [option()]) :: result()
  def await_settled(%Deployment{} = deployment, opts \\ []) do
    case Deployment.status(deployment) do
      :failed ->
        :ok

      _ ->
        deployment
        |> Deployment.server_instances()
        |> Enum.reduce_while(:ok, fn server, :ok ->
          check_server(server, deployment.controller, opts)
        end)
    end
  end

  defp check_server(
         %ServerInstance{operational_state: :running, pid: os_pid} = server,
         controller,
         opts
       )
       when is_integer(os_pid) do
    inspector = Keyword.get(opts, :inspector, &ProcStatus.probe/1)

    case inspector.(os_pid) do
      :alive -> {:cont, :ok}
      {:crashing, reason} -> await_crash(server, controller, reason, opts)
    end
  end

  defp check_server(_server, _controller, _opts), do: {:cont, :ok}

  defp await_crash(server, controller, reason, opts) do
    timeout = Keyword.get(opts, :timeout, @default_timeout)

    Logger.warning(
      "#{server.id} (pid=#{server.pid}) appears to be crashing (reason=#{reason}), " <>
        "blocking up to #{timeout}ms for crash event"
    )

    started = System.monotonic_time(:millisecond)

    case Controller.await_crash_event(controller, server.id, timeout) do
      :ok ->
        elapsed = System.monotonic_time(:millisecond) - started
        Logger.info("#{server.id} crash event received after #{elapsed}ms, proceeding")
        {:halt, :ok}

      :timeout ->
        message =
          "Server #{server.id} appears to be crashing (reason=#{reason}) but no crash " <>
            "event arrived within #{timeout}ms"

        Logger.error(message)
        {:halt, {:error, message}}
    end
  end
end
