defmodule Toast.ServerTestHelpers do
  @moduledoc false

  alias Toast.Process.ServerProcess

  def cleanup_server(pid) do
    if Process.alive?(pid) do
      try do
        ServerProcess.stop(pid, 2_000)
      catch
        :exit, _ -> :ok
      end
    end

    if Process.alive?(pid) do
      ref = Process.monitor(pid)

      try do
        GenServer.stop(pid, :normal, 1_000)
      catch
        :exit, _ -> :ok
      end

      receive do
        {:DOWN, ^ref, _, _, _} -> :ok
      after
        1_000 -> :ok
      end
    end
  end

  def os_process_alive?(os_pid) do
    match?({_, 0}, System.cmd("kill", ["-0", to_string(os_pid)], stderr_to_stdout: true))
  end
end
