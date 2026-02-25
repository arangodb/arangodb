defmodule ToastTest.CrashMonitor do
  @spec handle_crash(Toast.Deployment.t(), map()) :: :ok
  def handle_crash(_deployment, crash_info) do
    server_id = Map.get(crash_info, :server_id, "unknown")
    signal = Map.get(crash_info, :signal)
    exit_status = Map.get(crash_info, :exit_status)

    parts = ["Server #{server_id} crashed"]
    parts = if signal, do: parts ++ ["(signal: #{signal})"], else: parts
    parts = if exit_status, do: parts ++ ["exit_status=#{exit_status}"], else: parts

    ToastTest.Runner.abort!(Enum.join(parts, " "))
  end
end
