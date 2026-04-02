defmodule Toast.ServerTestHelpers do
  @moduledoc false

  alias Toast.Process.ServerProcess

  def cleanup_server(pid) do
    ServerProcess.shutdown(pid, 2_000)
  end

  def os_process_alive?(os_pid) do
    match?({_, 0}, System.cmd("kill", ["-0", to_string(os_pid)], stderr_to_stdout: true))
  end
end
