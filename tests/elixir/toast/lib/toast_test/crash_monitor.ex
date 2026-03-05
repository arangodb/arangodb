defmodule ToastTest.CrashMonitor do
  @moduledoc "Default on_crash callback that aborts the test run when a server crashes unexpectedly."

  @spec handle_crash(String.t(), Toast.Process.CrashInfo.t()) :: :ok
  def handle_crash(_server_id, crash_info) do
    signal = crash_info.signal
    exit_status = crash_info.exit_status

    message =
      [
        "Server crashed",
        if(signal, do: "(signal: #{signal})"),
        if(exit_status, do: "exit_status=#{exit_status}")
      ]
      |> Toast.Utils.compact_join(" ")

    ToastTest.Runner.abort!({:crash, message})
  end
end
