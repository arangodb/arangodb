################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule Toast.Diagnostics.Coredump.LLDB do
  @moduledoc "LLDB backend for coredump analysis."

  @behaviour Toast.Diagnostics.Coredump.Debugger

  alias Toast.Diagnostics.Coredump.Debugger

  @impl true
  def executable, do: "lldb"

  @impl true
  def command(binary_path, core_path) do
    [
      "-c",
      core_path,
      "-o",
      "settings set target.x86-disassembly-flavor intel",
      "-o",
      "register read --all",
      "-o",
      "disassemble --frame",
      "-o",
      "thread list",
      "-o",
      "thread backtrace all",
      "-o",
      "quit",
      "--",
      binary_path
    ]
  end

  @impl true
  def parse_output(output) do
    {registers, disassembly, output} = extract_preamble_sections(output)
    lines = String.split(output, "\n")

    # First pass: build thread# → os_id map from `thread list` output
    # (which includes tid even when `thread backtrace all` does not).
    tid_map = parse_tid_map(lines)

    init_acc = %{threads: [], current: nil, signal: nil, crash_thread: nil}

    %{threads: threads, signal: signal, crash_thread: crash_thread} =
      lines
      |> Enum.reduce(init_acc, fn line, acc ->
        acc
        |> maybe_parse_thread_header(line, tid_map)
        |> maybe_parse_frame(line)
      end)
      |> Debugger.flush_current_thread()

    threads =
      threads
      |> Debugger.deduplicate_threads()
      |> Debugger.filter_threads(crash_thread)

    %{
      signal: signal,
      faulting_address: nil,
      registers: registers,
      disassembly: disassembly,
      threads: Enum.reverse(threads),
      crash_thread: crash_thread
    }
  end

  # Extract register and disassembly sections from LLDB output.
  # These appear as `(lldb) command\n...output...` blocks.
  # When no `(lldb)` prompts exist (e.g., in tests with raw output), pass through unchanged.
  # Extract register and disassembly sections from LLDB output.
  # Non-matching lines (register values, disassembly instructions) are harmlessly
  # skipped by the thread/frame parser, so no stripping needed.
  @register_section ~r/\(lldb\)\s*register read[^\n]*\n(.*?)(?=\(lldb\)|\z)/s
  @disassembly_section ~r/\(lldb\)\s*disassemble[^\n]*\n(.*?)(?=\(lldb\)|\z)/s

  defp extract_preamble_sections(output) do
    registers =
      case Regex.run(@register_section, output) do
        [_, body] -> non_empty_trim(body)
        _ -> nil
      end

    disassembly =
      case Regex.run(@disassembly_section, output) do
        [_, body] -> non_empty_trim(body)
        _ -> nil
      end

    {registers, disassembly, output}
  end

  defp non_empty_trim(str) do
    trimmed = String.trim(str)
    if trimmed == "", do: nil, else: trimmed
  end

  @tid_pattern ~r/thread\s+#(\d+).*?tid\s*=\s*(0x[0-9a-fA-F]+|\d+)/i

  defp parse_tid_map(lines) do
    Enum.reduce(lines, %{}, fn line, acc ->
      case Regex.run(@tid_pattern, line) do
        [_, id_str, tid] -> Map.put_new(acc, String.to_integer(id_str), tid)
        _ -> acc
      end
    end)
  end

  defp maybe_parse_thread_header(acc, line, tid_map) do
    case Regex.run(~r/^\s*(\*?)\s*thread\s+#(\d+)/i, line) do
      [_, star, id_str] ->
        acc = Debugger.flush_current_thread(acc)
        thread_id = String.to_integer(id_str)
        is_crash = star == "*"
        signal = if(is_crash, do: extract_signal(line), else: acc.signal)
        crash_thread = if(is_crash, do: thread_id, else: acc.crash_thread)
        os_id = extract_tid(line) || Map.get(tid_map, thread_id)

        %{
          acc
          | current: %{id: thread_id, os_id: os_id, frames: []},
            signal: signal || acc.signal,
            crash_thread: crash_thread
        }

      _ ->
        acc
    end
  end

  defp extract_tid(line) do
    case Regex.run(~r/tid\s*=\s*(0x[0-9a-fA-F]+|\d+)/, line) do
      [_, tid] -> tid
      _ -> nil
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

  @frame_with_location ~r/frame\s+#\d+:\s+0x[0-9a-fA-F]+\s+\S+`(.+?)\s+at\s+(.+):(\d+)/
  @frame_without_location ~r/frame\s+#\d+:\s+0x[0-9a-fA-F]+\s+\S+`(.+)/

  defp parse_frame_line(line) do
    case Regex.run(@frame_with_location, line) do
      [_, func, file, line_num] ->
        %{function: clean_function(func), file: file, line: String.to_integer(line_num)}

      nil ->
        case Regex.run(@frame_without_location, line) do
          [_, func] -> %{function: clean_function(func), file: nil, line: nil}
          nil -> nil
        end
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
