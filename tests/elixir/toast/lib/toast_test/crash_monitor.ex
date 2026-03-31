defmodule ToastTest.CrashMonitor do
  @moduledoc """
  Aborts the test run when a server crashes unexpectedly.

  Called directly by the deployment controller. Silently does nothing
  if the test infrastructure (Abort ETS table) is not running.
  """

  @spec handle_crash(String.t(), Toast.Process.CrashInfo.t()) :: :ok
  def handle_crash(server_id, %Toast.Process.CrashInfo{signal: signal, exit_status: exit_status}) do
    message =
      [
        "Server crashed: #{server_id}",
        if(signal, do: "(signal: #{signal})"),
        if(exit_status, do: "exit_status=#{exit_status}")
      ]
      |> Toast.Utils.compact_join(" ")

    # abort!/1 calls :ets.insert_new which raises ArgumentError if the
    # Abort ETS table hasn't been created yet (no active suite run).
    ToastTest.Abort.abort!({:crash, message})
    ToastTest.Abort.kill_test_pid()
  rescue
    ArgumentError -> :ok
  end
end
