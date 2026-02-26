defmodule Toast.Diagnostics.Coredump.LLDB do
  @moduledoc "LLDB backend for coredump analysis."

  @behaviour Toast.Diagnostics.Coredump.Debugger

  alias Toast.Diagnostics.Coredump.Debugger

  @impl true
  def executable, do: "lldb"

  @impl true
  def command(binary_path, core_path) do
    ["-c", core_path, "-o", "thread backtrace all", "-o", "quit", "--", binary_path]
  end

  @impl true
  def parse_output(output) do
    lines = String.split(output, "\n")

    %{threads: threads, signal: signal, crash_thread: crash_thread} =
      Enum.reduce(lines, %{threads: [], current: nil, signal: nil, crash_thread: nil}, fn line, acc ->
        acc
        |> maybe_parse_thread_header(line)
        |> maybe_parse_frame(line)
      end)
      |> Debugger.flush_current_thread()

    threads = Debugger.filter_threads(threads, crash_thread)

    %{
      signal: signal,
      faulting_address: nil,
      threads: Enum.reverse(threads),
      crash_thread: crash_thread
    }
  end

  defp maybe_parse_thread_header(acc, line) do
    case Regex.run(~r/^\s*(\*?)\s*thread\s+#(\d+)/i, line) do
      [_, star, id_str] ->
        acc = Debugger.flush_current_thread(acc)
        thread_id = String.to_integer(id_str)
        is_crash = star == "*"
        signal = if(is_crash, do: extract_signal(line), else: acc.signal)
        crash_thread = if(is_crash, do: thread_id, else: acc.crash_thread)

        %{acc | current: %{id: thread_id, frames: []}, signal: signal || acc.signal, crash_thread: crash_thread}

      _ ->
        acc
    end
  end

  defp maybe_parse_frame(acc, line) do
    case parse_frame_line(line) do
      nil ->
        acc

      frame ->
        case acc.current do
          nil -> acc
          current -> %{acc | current: %{current | frames: [frame | current.frames]}}
        end
    end
  end

  defp parse_frame_line(line) do
    # Pattern: frame #N: 0x<addr> <module>`<function> at <file>:<line>
    # Pattern: frame #N: 0x<addr> <module>`<function>
    cond do
      match = Regex.run(~r/frame\s+#\d+:\s+0x[0-9a-fA-F]+\s+\S+`(.+?)\s+at\s+(.+):(\d+)/, line) ->
        [_, func, file, line_num] = match
        %{function: clean_function(func), file: file, line: String.to_integer(line_num)}

      match = Regex.run(~r/frame\s+#\d+:\s+0x[0-9a-fA-F]+\s+\S+`(.+)/, line) ->
        [_, func] = match
        %{function: clean_function(func), file: nil, line: nil}

      true ->
        nil
    end
  end

  defp clean_function(func) do
    func
    |> String.trim()
    |> String.replace(~r/\s+at\s+.*$/, "")
  end

  defp extract_signal(line) do
    case Regex.run(~r/stop reason = signal (SIG\w+)/, line) do
      [_, name] -> name
      _ -> nil
    end
  end
end
