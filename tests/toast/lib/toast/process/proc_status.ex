defmodule Toast.Process.ProcStatus do
  @moduledoc """
  Linux-specific probe that classifies an OS process via `/proc/<pid>/status`.

  Used to detect whether a server process has crashed and is being dumped by the
  kernel — a state in which `kill(pid, 0)` still reports the process as alive
  and `waitpid` has not yet returned, so erlexec's `:DOWN` notification has not
  yet been delivered. During a coredump this window can last minutes; without
  an explicit check, Toast would incorrectly advance (run the next test or
  finalize the suite) against a server that is already gone.
  """

  @type crash_reason :: :core_dumping | :zombie | :proc_missing
  @type result :: :alive | {:crashing, crash_reason()}

  @spec probe(pos_integer()) :: result()
  def probe(os_pid) when is_integer(os_pid) and os_pid > 0 do
    case File.read("/proc/#{os_pid}/status") do
      {:ok, content} -> classify(content)
      {:error, :enoent} -> {:crashing, :proc_missing}
    end
  end

  @doc false
  @spec classify(binary()) :: result()
  def classify(content) when is_binary(content) do
    cond do
      Regex.match?(~r/^State:\s+Z\b/m, content) -> {:crashing, :zombie}
      Regex.match?(~r/^CoreDumping:\s+1\b/m, content) -> {:crashing, :core_dumping}
      true -> :alive
    end
  end
end
