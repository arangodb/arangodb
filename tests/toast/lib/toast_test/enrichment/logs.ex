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

defmodule ToastTest.Enrichment.Logs do
  @moduledoc """
  Extract structured log entries from ArangoDB JSON log files.

  Log files use one JSON object per line (NDJSON format). Each entry is
  parsed into a map with atoms for `time`, `level`, and `role` fields.
  """

  import Toast.Utils, only: [maybe_put: 3]

  # Read backwards in 64 KB chunks
  @chunk_size 64 * 1024

  @level_map %{
    "FATAL" => :fatal,
    "ERR" => :error,
    "ERROR" => :error,
    "WARN" => :warning,
    "WARNING" => :warning,
    "INFO" => :info,
    "DEBUG" => :debug,
    "TRACE" => :trace
  }

  @role_map %{
    "C" => :coordinator,
    "P" => :dbserver,
    "A" => :agent,
    "S" => :single
  }

  @doc """
  Parse a single JSON log line into a structured map.

  Returns `{:ok, entry}` or `:error` if the line is not valid JSON or
  lacks a `time` field.
  """
  @spec parse_line(String.t()) :: {:ok, map()} | :error
  def parse_line(line) do
    line = String.trim_trailing(line, "\n")

    with {:ok, raw} <- json_decode(line),
         %{"time" => time_str} <- raw,
         {:ok, dt, _offset} <- DateTime.from_iso8601(time_str) do
      {:ok, build_entry(raw, DateTime.to_unix(dt, :microsecond))}
    else
      _ -> :error
    end
  end

  defp json_decode(line) do
    {:ok, :json.decode(line)}
  rescue
    _ -> :error
  end

  defp build_entry(raw, time) do
    %{time: time, message: raw["message"] || ""}
    |> maybe_put(:level, Map.get(@level_map, raw["level"]))
    |> maybe_put(:role, Map.get(@role_map, raw["role"]))
    |> maybe_put_atom(:topic, raw["topic"])
    |> maybe_put(:id, raw["id"])
    |> maybe_put(:pid, raw["pid"])
    |> maybe_put(:file, raw["file"])
    |> maybe_put(:line, raw["line"])
    |> maybe_put(:function, raw["function"])
  end

  defp maybe_put_atom(entry, _key, nil), do: entry

  # String.to_atom/1 is safe here: ArangoDB log topics are a fixed, bounded set
  # defined in the server binary. Values come from our own process logs, not
  # untrusted external input, so atom table exhaustion is not a concern.
  defp maybe_put_atom(entry, key, value), do: Map.put(entry, key, String.to_atom(value))

  @doc """
  Return the timestamp (Unix microseconds) of the first crash log entry,
  or `nil` if the log file has no crash block at its end.
  """
  @spec extract_crash_timestamp(Path.t()) :: Toast.timestamp() | nil
  def extract_crash_timestamp(path) do
    case extract_crash_lines(path) do
      [%{time: time} | _] -> time
      [] -> nil
    end
  end

  @doc """
  Return the last contiguous block of crash-topic log entries.

  Reads the file backwards to efficiently find the most recent crash output.
  Only returns entries when the crash block is at the very end of the log
  (the crash handler writes these as the last thing before the process dies).

  Returns `[]` if the file does not exist or contains no crash entries at the end.
  """
  @spec extract_crash_lines(Path.t()) :: [map()]
  def extract_crash_lines(path) do
    case File.open(path, [:read, :binary]) do
      {:ok, device} ->
        try do
          {:ok, file_size} = :file.position(device, :eof)
          read_crash_entries_backwards(device, file_size)
        after
          File.close(device)
        end

      {:error, _} ->
        []
    end
  end

  defp read_crash_entries_backwards(_device, 0), do: []

  defp read_crash_entries_backwards(device, file_size) do
    scan_backwards(device, file_size, "", [])
  end

  defp scan_backwards(_device, 0, leftover, acc) do
    finalize_crash_block(leftover, acc)
  end

  defp scan_backwards(device, pos, leftover, acc) do
    read_start = max(pos - @chunk_size, 0)
    bytes_to_read = pos - read_start

    {:ok, _} = :file.position(device, read_start)
    {:ok, chunk} = :file.read(device, bytes_to_read)

    data = chunk <> leftover
    [new_leftover | lines] = String.split(data, "\n")

    case process_lines_reverse(Enum.reverse(lines), acc) do
      {:done, result} -> result
      {:continue, new_acc} -> scan_backwards(device, read_start, new_leftover, new_acc)
    end
  end

  defp process_lines_reverse([], acc), do: {:continue, acc}

  defp process_lines_reverse([line | rest], []) do
    cond do
      line == "" ->
        process_lines_reverse(rest, [])

      crash_entry?(line) ->
        case parse_line(line) do
          {:ok, entry} -> process_lines_reverse(rest, [entry])
          :error -> {:done, []}
        end

      true ->
        {:done, []}
    end
  end

  defp process_lines_reverse([line | rest], acc) do
    if crash_entry?(line) do
      case parse_line(line) do
        {:ok, entry} -> process_lines_reverse(rest, [entry | acc])
        :error -> {:done, acc}
      end
    else
      {:done, acc}
    end
  end

  defp finalize_crash_block(leftover, acc) do
    if crash_entry?(leftover) do
      case parse_line(leftover) do
        {:ok, entry} -> [entry | acc]
        :error -> acc
      end
    else
      acc
    end
  end

  defp crash_entry?(line) do
    # Quick string check before attempting JSON parse
    String.contains?(line, "\"crash\"")
  end

  @doc """
  Extract log entries for multiple sorted, non-overlapping time windows in a single pass.

  Windows are `{start_us, end_us}` tuples (Unix microseconds).
  Returns a list of entry lists (one per window), in the same order as the input windows.
  Empty list for windows with no matching entries.
  """
  @spec extract_windows([{Toast.timestamp(), Toast.timestamp()}], Path.t()) :: [[map()]]
  def extract_windows([], _path), do: []

  def extract_windows(windows, path) do
    case File.open(path, [:read, :utf8]) do
      {:ok, device} ->
        try do
          collect_multi_windows(device, windows, [], [])
        after
          File.close(device)
        end

      {:error, _} ->
        List.duplicate([], length(windows))
    end
  end

  # Single-pass multi-window scan across a sorted log file.
  #
  # Invariants:
  #   - `windows` is a non-empty list of non-overlapping `{start_us, end_us}` tuples in ascending order.
  #   - `acc` accumulates entries for the *current* (head) window, in reverse order.
  #   - `results` is a list of already-finalized entry lists (one per completed window), in reverse order.
  #   - The log file is read forward; entries are assumed to be monotonically non-decreasing by time.
  #
  # Transitions:
  #   - Entry time < window start  → discard entry, advance file.
  #   - Entry time in window       → append to acc, advance file.
  #   - Entry time > window end    → finalize acc into results, advance to next window without
  #                                  reading a new line (`recheck_entry` re-evaluates the same entry).
  #   - EOF                        → finalize acc, pad remaining windows with empty lists.
  #   - Windows exhausted          → done.

  # All windows consumed — finalize current accumulator and pad remaining
  defp collect_multi_windows(_device, [], acc, results) do
    Enum.reverse([Enum.reverse(acc) | results])
  end

  defp collect_multi_windows(
         device,
         [{_win_start, _win_end} | rest_windows] = windows,
         acc,
         results
       ) do
    case IO.read(device, :line) do
      line when is_binary(line) ->
        classify_line(device, windows, rest_windows, line, acc, results)

      _ ->
        remaining = [Enum.reverse(acc) | results]
        padded = List.duplicate([], length(rest_windows))
        Enum.reverse(remaining) ++ padded
    end
  end

  defp classify_line(device, windows, rest_windows, line, acc, results) do
    {win_start, win_end} = hd(windows)

    case parse_line(line) do
      {:ok, entry} ->
        cond do
          entry.time < win_start ->
            collect_multi_windows(device, windows, acc, results)

          entry.time > win_end ->
            recheck_entry(device, rest_windows, entry, results, acc)

          true ->
            collect_multi_windows(device, windows, [entry | acc], results)
        end

      :error ->
        collect_multi_windows(device, windows, acc, results)
    end
  end

  defp recheck_entry(_device, [], _entry, results, acc) do
    Enum.reverse([Enum.reverse(acc) | results])
  end

  defp recheck_entry(
         device,
         [{win_start, win_end} | rest_windows] = windows,
         entry,
         results,
         prev_acc
       ) do
    results = [Enum.reverse(prev_acc) | results]

    cond do
      entry.time < win_start ->
        collect_multi_windows(device, windows, [], results)

      entry.time > win_end ->
        recheck_entry(device, rest_windows, entry, results, [])

      true ->
        collect_multi_windows(device, windows, [entry], results)
    end
  end
end
