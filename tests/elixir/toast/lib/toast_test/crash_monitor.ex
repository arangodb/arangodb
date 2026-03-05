defmodule ToastTest.CrashMonitor do
  @moduledoc "Default on_crash callback that aborts the test run when a server crashes unexpectedly."

  @spec handle_crash(Toast.Deployment.t(), map()) :: :ok
  def handle_crash(_deployment, crash_info) do
    server_id = Map.get(crash_info, :server_id, "unknown")
    signal = Map.get(crash_info, :signal)
    exit_status = Map.get(crash_info, :exit_status)

    message =
      [
        "Server #{server_id} crashed",
        if(signal, do: "(signal: #{signal})"),
        if(exit_status, do: "exit_status=#{exit_status}")
      ]
      |> Toast.Utils.compact_join(" ")

    ToastTest.Runner.abort!({:crash, message})
  end
end
