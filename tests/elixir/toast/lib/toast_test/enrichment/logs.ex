defmodule ToastTest.Enrichment.Logs do
  @moduledoc """
  Extract excerpts from ArangoDB log files.

  Supports time-windowed extraction and category-filtered extraction.
  Assumes log lines are chronologically ordered for windowed reads.
  """

  # Read backwards in 64 KB chunks
  @chunk_size 64 * 1024

  @doc """
  Return the last contiguous block of `{crash}` lines from the log file.

  Reads the file backwards to efficiently find the most recent crash output,
  ignoring earlier crashes (e.g. from resilience tests with expected crashes).

  Returns `""` if the file does not exist or contains no `{crash}` lines.
  """
  @spec extract_crash_lines(Path.t()) :: String.t()
  def extract_crash_lines(path) do
    case File.open(path, [:read, :binary]) do
      {:ok, device} ->
        try do
          {:ok, file_size} = :file.position(device, :eof)
          read_crash_lines_backwards(device, file_size)
        after
          File.close(device)
        end

      {:error, _} ->
        ""
    end
  end

  defp read_crash_lines_backwards(_device, 0), do: ""

  defp read_crash_lines_backwards(device, file_size) do
    # Scan backwards through the file collecting {crash} lines.
    # We accumulate lines from the last contiguous crash block.
    scan_backwards(device, file_size, "", [])
  end

  # Scans from end of file in chunks.  `leftover` holds any partial line
  # at the front of the previous chunk (i.e. text before the first \n).
  defp scan_backwards(_device, 0, leftover, acc) do
    # Reached beginning of file — process any remaining leftover as a line
    finalize_crash_block(leftover, acc)
  end

  defp scan_backwards(device, pos, leftover, acc) do
    read_start = max(pos - @chunk_size, 0)
    bytes_to_read = pos - read_start

    {:ok, _} = :file.position(device, read_start)
    {:ok, chunk} = :file.read(device, bytes_to_read)

    # Prepend leftover from previous chunk to form a complete line
    data = chunk <> leftover

    # Split into lines — first element may be partial if read_start > 0
    [new_leftover | lines] = String.split(data, "\n")

    # Process lines from bottom to top (they're in file order after split,
    # so reverse to process last lines first)
    case process_lines_reverse(Enum.reverse(lines), acc) do
      {:done, result} -> result
      {:continue, new_acc} -> scan_backwards(device, read_start, new_leftover, new_acc)
    end
  end

  # Process lines from end-of-chunk toward start (already reversed).
  # States:
  #   acc == []  → haven't found any crash lines yet, skip non-crash
  #   acc != []  → in a crash block, stop at first non-crash line
  defp process_lines_reverse([], acc), do: {:continue, acc}

  defp process_lines_reverse([line | rest], []) do
    if crash_line?(line) do
      process_lines_reverse(rest, [line])
    else
      process_lines_reverse(rest, [])
    end
  end

  defp process_lines_reverse([line | rest], acc) do
    if crash_line?(line) do
      process_lines_reverse(rest, [line | acc])
    else
      # Hit a non-crash line while we had crash lines — block is complete
      {:done, join_block(acc)}
    end
  end

  defp finalize_crash_block(leftover, acc) do
    if crash_line?(leftover) do
      join_block([leftover | acc])
    else
      join_block(acc)
    end
  end

  defp crash_line?(line), do: String.contains?(line, "{crash}")

  defp join_block([]), do: ""
  defp join_block(lines), do: Enum.join(lines, "\n")

  @doc """
  Return all log lines whose timestamp falls in [start, finish].

  Returns `""` if the file does not exist or contains no matching lines.
  """
  @spec extract_window(Path.t(), DateTime.t(), DateTime.t()) :: String.t()
  def extract_window(path, start_dt, end_dt) do
    case File.open(path, [:read, :utf8]) do
      {:ok, device} ->
        try do
          collect_lines(device, start_dt, end_dt, [])
        after
          File.close(device)
        end

      {:error, _} ->
        ""
    end
  end

  defp collect_lines(device, start_dt, end_dt, acc) do
    case IO.read(device, :line) do
      :eof -> finalize(acc)
      {:error, _} -> finalize(acc)
      line -> process_line(line, device, start_dt, end_dt, acc)
    end
  end

  defp process_line(line, device, start_dt, end_dt, acc) do
    case parse_timestamp(line) do
      {:ok, ts} -> apply_window(ts, line, device, start_dt, end_dt, acc)
      :error -> collect_with_continuation(line, device, start_dt, end_dt, acc)
    end
  end

  defp apply_window(ts, line, device, start_dt, end_dt, acc) do
    cond do
      DateTime.compare(ts, start_dt) == :lt ->
        collect_lines(device, start_dt, end_dt, acc)

      DateTime.compare(ts, end_dt) == :gt ->
        finalize(acc)

      true ->
        collect_lines(device, start_dt, end_dt, [line | acc])
    end
  end

  # Non-timestamped lines: include only if we're already inside the window
  defp collect_with_continuation(line, device, start_dt, end_dt, [_ | _] = acc),
    do: collect_lines(device, start_dt, end_dt, [line | acc])

  defp collect_with_continuation(_line, device, start_dt, end_dt, []),
    do: collect_lines(device, start_dt, end_dt, [])

  defp finalize([]), do: ""

  defp finalize(acc) do
    acc
    |> Enum.reverse()
    |> Enum.map_join("\n", &String.trim_trailing(&1, "\n"))
  end

  defp parse_timestamp(line) do
    # ArangoDB format: "2026-01-15T12:00:00Z [pid] LEVEL [topic] message"
    # Extract the timestamp (everything before the first space)
    case String.split(line, " ", parts: 2) do
      [ts_str | _] ->
        case DateTime.from_iso8601(ts_str) do
          {:ok, dt, _offset} -> {:ok, dt}
          _ -> :error
        end

      _ ->
        :error
    end
  end
end
