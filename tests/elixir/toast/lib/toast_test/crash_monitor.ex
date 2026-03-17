defmodule ToastTest.CrashMonitor do
  @moduledoc "Default on_crash callback that aborts the test run when a server crashes unexpectedly."

  @spec handle_crash(String.t(), Toast.Process.CrashInfo.t()) :: :ok
  def handle_crash(_server_id, %Toast.Process.CrashInfo{signal: signal, exit_status: exit_status}) do
    message =
      [
        "Server crashed",
        if(signal, do: "(signal: #{signal})"),
        if(exit_status, do: "exit_status=#{exit_status}")
      ]
      |> Toast.Utils.compact_join(" ")

    ToastTest.Abort.abort!({:crash, message})
    ToastTest.Abort.kill_test_pid()
  end
end
