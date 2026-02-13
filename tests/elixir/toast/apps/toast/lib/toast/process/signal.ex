defmodule Toast.Process.Signal do
  @moduledoc """
  Send POSIX signals to OS processes.
  """

  @doc """
  Send SIGTERM to the given OS pid.
  Returns :ok on success, {:error, reason} on failure.
  """
  @spec term(pos_integer()) :: :ok | {:error, String.t()}
  def term(os_pid) when is_integer(os_pid) and os_pid > 0 do
    send_signal(os_pid, "TERM")
  end

  @doc """
  Send SIGKILL to the given OS pid.
  """
  @spec kill(pos_integer()) :: :ok | {:error, String.t()}
  def kill(os_pid) when is_integer(os_pid) and os_pid > 0 do
    send_signal(os_pid, "KILL")
  end

  @doc """
  Send SIGKILL to the process group of the given OS pid.
  This kills all processes in the group (negative pid to kill).
  """
  @spec kill_group(pos_integer()) :: :ok | {:error, String.t()}
  def kill_group(os_pid) when is_integer(os_pid) and os_pid > 0 do
    send_signal(-os_pid, "KILL")
  end

  @doc """
  Check if an OS process is alive.
  """
  @spec alive?(pos_integer()) :: boolean()
  def alive?(os_pid) when is_integer(os_pid) and os_pid > 0 do
    case System.cmd("kill", ["-0", to_string(os_pid)], stderr_to_stdout: true) do
      {_, 0} -> true
      _ -> false
    end
  end

  defp send_signal(pid, signal) do
    case System.cmd("kill", ["-#{signal}", to_string(pid)], stderr_to_stdout: true) do
      {_, 0} -> :ok
      {output, _} -> {:error, String.trim(output)}
    end
  end
end
