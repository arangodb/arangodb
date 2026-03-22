defmodule ToastTest.Enrichment.Logs do
  @moduledoc """
  Extract structured log entries from ArangoDB JSON log files.

  Log files use one JSON object per line (NDJSON format). Each entry is
  parsed into a map with atoms for `time`, `level`, and `role` fields.
  """

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
    entry = %{time: time, message: raw["message"] || ""}

    entry =
      case Map.get(@level_map, raw["level"]) do
        nil -> entry
        level -> Map.put(entry, :level, level)
      end

    entry =
      case Map.get(@role_map, raw["role"]) do
        nil -> entry
        role -> Map.put(entry, :role, role)
      end

    entry = maybe_put_atom(entry, :topic, raw["topic"])
    entry = maybe_put(entry, :id, raw["id"])
    entry = maybe_put(entry, :pid, raw["pid"])
    entry = maybe_put(entry, :file, raw["file"])
    entry = maybe_put(entry, :line, raw["line"])
    maybe_put(entry, :function, raw["function"])
  end

  defp maybe_put(entry, _key, nil), do: entry
  defp maybe_put(entry, key, value), do: Map.put(entry, key, value)

  defp maybe_put_atom(entry, _key, nil), do: entry
  defp maybe_put_atom(entry, key, value), do: Map.put(entry, key, String.to_atom(value))

  @doc """
  Return the last contiguous block of crash-topic log entries.

  Reads the file backwards to efficiently find the most recent crash output,
  ignoring earlier crashes (e.g. from resilience tests with expected crashes).

  Returns `[]` if the file does not exist or contains no crash entries.
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
    if crash_entry?(line) do
      case parse_line(line) do
        {:ok, entry} -> process_lines_reverse(rest, [entry])
        :error -> process_lines_reverse(rest, [])
      end
    else
      process_lines_reverse(rest, [])
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

  Returns a list of entry lists (one per window), in the same order as the input windows.
  Empty list for windows with no matching entries.
  """
  @spec extract_windows([{DateTime.t(), DateTime.t()}], Path.t()) :: [[map()]]
  def extract_windows([], _path), do: []

  def extract_windows(windows, path) do
    unix_windows =
      Enum.map(windows, fn {start_dt, end_dt} ->
        {DateTime.to_unix(start_dt, :microsecond), DateTime.to_unix(end_dt, :microsecond)}
      end)

    case File.open(path, [:read, :utf8]) do
      {:ok, device} ->
        try do
          collect_multi_windows(device, unix_windows, [], [])
        after
          File.close(device)
        end

      {:error, _} ->
        List.duplicate([], length(windows))
    end
  end

  # All windows consumed — finalize current accumulator and pad remaining
  defp collect_multi_windows(_device, [], acc, results) do
    Enum.reverse([Enum.reverse(acc) | results])
  end

  defp collect_multi_windows(
         device,
         [{win_start, win_end} | rest_windows] = windows,
         acc,
         results
       ) do
    case IO.read(device, :line) do
      line when is_binary(line) ->
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

      _ ->
        remaining = [Enum.reverse(acc) | results]
        padded = List.duplicate([], length(rest_windows))
        Enum.reverse(remaining) ++ padded
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
