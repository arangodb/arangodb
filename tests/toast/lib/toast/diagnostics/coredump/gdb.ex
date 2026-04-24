defmodule Toast.Diagnostics.Coredump.GDB do
  @moduledoc "GDB backend for coredump analysis."

  @behaviour Toast.Diagnostics.Coredump.Debugger

  alias Toast.Diagnostics.Coredump.Debugger

  @impl true
  def executable, do: "gdb"

  @impl true
  def command(binary_path, core_path) do
    [
      "-batch",
      "-ex",
      "set disassembly-flavor intel",
      "-ex",
      "info registers",
      "-ex",
      "disassemble",
      "-ex",
      "thread apply all bt full",
      "-ex",
      "quit",
      binary_path,
      core_path
    ]
  end

  @impl true
  def parse_output(output) do
    {registers, disassembly, output} = extract_preamble_sections(output)
    lines = String.split(output, "\n")

    init_acc = %{threads: [], current: nil, signal: nil, faulting_address: nil, crash_thread: nil}

    %{threads: threads, signal: signal, faulting_address: addr, crash_thread: crash_thread} =
      lines
      |> Enum.reduce(init_acc, fn line, acc ->
        acc
        |> maybe_parse_signal(line)
        |> maybe_parse_thread_header(line)
        |> maybe_parse_frame(line)
      end)
      |> Debugger.flush_current_thread()

    threads =
      threads
      |> Debugger.deduplicate_threads()
      |> Debugger.filter_threads(crash_thread)

    %{
      signal: signal,
      faulting_address: addr,
      registers: registers,
      disassembly: disassembly,
      threads: Enum.reverse(threads),
      crash_thread: crash_thread
    }
  end

  # Extract "info registers" and "Dump of assembler code" sections that appear
  # before the thread backtraces.  Returns {registers, disassembly, remaining_output}.
  @registers_pattern ~r/(?:^|\n)((?:[a-z][a-z0-9_]+\s+0x[0-9a-fA-F]+\s+.*\n?)+)/
  @disassembly_pattern ~r/(Dump of assembler code for function .+?End of assembler dump\.)/s

  defp extract_preamble_sections(output) do
    registers =
      case Regex.run(@registers_pattern, output) do
        [_, section] -> String.trim(section)
        _ -> nil
      end

    disassembly =
      case Regex.run(@disassembly_pattern, output) do
        [_, section] -> String.trim(section)
        _ -> nil
      end

    {registers, disassembly, output}
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
    case Regex.run(~r/^Thread\s+(\d+)\s.*\(/, line) do
      [_, id_str] ->
        thread_id = String.to_integer(id_str)
        os_id = extract_lwp(line)

        # When GDB outputs frames before any "Thread N" header, we create an
        # implicit thread.  If the first explicit header matches that implicit
        # thread's id, replace it — the explicit "thread apply all bt" section
        # is a superset of the pre-header snippet.
        case acc.current do
          %{id: ^thread_id} ->
            %{acc | current: %{id: thread_id, os_id: os_id, frames: []}}

          _ ->
            acc = Debugger.flush_current_thread(acc)
            %{acc | current: %{id: thread_id, os_id: os_id, frames: []}}
        end

      _ ->
        acc
    end
  end

  defp extract_lwp(line) do
    case Regex.run(~r/LWP\s+(\d+)/, line) do
      [_, lwp] -> lwp
      _ -> nil
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
            current = %{id: crash_thread, os_id: nil, frames: [frame]}
            %{acc | current: current, crash_thread: crash_thread}

          current ->
            %{acc | current: %{current | frames: [frame | current.frames]}}
        end
    end
  end

  @frame_with_location ~r/^#\d+\s+(?:0x[0-9a-fA-F]+\s+in\s+)?(.+?)\s+\(.*?\)\s+at\s+(.+):(\d+)/
  @frame_without_location ~r/^#\d+\s+(?:0x[0-9a-fA-F]+\s+in\s+)?(.+?)\s+\(/

  defp parse_frame_line(line) do
    case Regex.run(@frame_with_location, line) do
      [_, func, file, line_num] ->
        %{function: String.trim(func), file: file, line: String.to_integer(line_num)}

      nil ->
        case Regex.run(@frame_without_location, line) do
          [_, func] -> %{function: String.trim(func), file: nil, line: nil}
          nil -> nil
        end
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
