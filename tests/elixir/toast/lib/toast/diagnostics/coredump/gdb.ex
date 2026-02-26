defmodule Toast.Diagnostics.Coredump.GDB do
  @moduledoc "GDB backend for coredump analysis."

  @behaviour Toast.Diagnostics.Coredump.Debugger

  alias Toast.Diagnostics.Coredump.Debugger

  @impl true
  def executable, do: "gdb"

  @impl true
  def command(binary_path, core_path) do
    ["-batch", "-ex", "thread apply all bt full", "-ex", "quit", binary_path, core_path]
  end

  @impl true
  def parse_output(output) do
    lines = String.split(output, "\n")

    %{threads: threads, signal: signal, faulting_address: addr, crash_thread: crash_thread} =
      Enum.reduce(lines, %{threads: [], current: nil, signal: nil, faulting_address: nil, crash_thread: nil}, fn line, acc ->
        acc
        |> maybe_parse_signal(line)
        |> maybe_parse_thread_header(line)
        |> maybe_parse_frame(line)
      end)
      |> Debugger.flush_current_thread()

    threads = Debugger.filter_threads(threads, crash_thread)

    %{
      signal: signal,
      faulting_address: addr,
      threads: Enum.reverse(threads),
      crash_thread: crash_thread
    }
  end

  defp maybe_parse_signal(acc, line) do
    cond do
      acc.signal == nil && String.contains?(line, "Program terminated with signal") ->
        signal = extract_signal_name(line)
        addr = extract_faulting_address(line)
        %{acc | signal: signal, faulting_address: addr}

      acc.faulting_address == nil && String.contains?(line, "si_addr") ->
        case Regex.run(~r/si_addr\s*=\s*(0x[0-9a-fA-F]+)/, line) do
          [_, addr] -> %{acc | faulting_address: addr}
          _ -> acc
        end

      true ->
        acc
    end
  end

  defp maybe_parse_thread_header(acc, line) do
    case Regex.run(~r/^Thread\s+(\d+)\s+\(/, line) do
      [_, id_str] ->
        acc = Debugger.flush_current_thread(acc)
        thread_id = String.to_integer(id_str)
        %{acc | current: %{id: thread_id, frames: []}}

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
          nil ->
            # GDB may output the crashing thread's backtrace before any "Thread N"
            # header. We attribute these frames to thread 1 (the conventional crash
            # thread ID). If an explicit "Thread 1" header follows, its frames will
            # be collected as a separate thread entry.
            crash_thread = acc.crash_thread || 1
            current = %{id: crash_thread, frames: [frame]}
            %{acc | current: current, crash_thread: crash_thread}

          current ->
            %{acc | current: %{current | frames: [frame | current.frames]}}
        end
    end
  end

  defp parse_frame_line(line) do
    # Pattern: #N  0x<addr> in <function> (<args>) at <file>:<line>
    # Pattern: #N  0x<addr> in <function> ()
    # Pattern: #N  <function> (<args>) at <file>:<line>
    cond do
      match = Regex.run(~r/^#\d+\s+(?:0x[0-9a-fA-F]+\s+in\s+)?(.+?)\s+\(.*?\)\s+at\s+(.+):(\d+)/, line) ->
        [_, func, file, line_num] = match
        %{function: String.trim(func), file: file, line: String.to_integer(line_num)}

      match = Regex.run(~r/^#\d+\s+(?:0x[0-9a-fA-F]+\s+in\s+)?(.+?)\s+\(/, line) ->
        [_, func] = match
        %{function: String.trim(func), file: nil, line: nil}

      true ->
        nil
    end
  end

  defp extract_signal_name(line) do
    case Regex.run(~r/signal\s+(SIG\w+)/, line) do
      [_, name] -> name
      _ -> nil
    end
  end

  defp extract_faulting_address(line) do
    case Regex.run(~r/address\s+(0x[0-9a-fA-F]+)/, line) do
      [_, addr] -> addr
      _ -> nil
    end
  end
end
