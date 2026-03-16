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
  Extract log lines for multiple sorted, non-overlapping time windows in a single pass.

  Returns a list of strings (one per window), in the same order as the input windows.
  Empty string for windows with no matching lines.
  """
  @spec extract_windows([{DateTime.t(), DateTime.t()}], Path.t()) :: [String.t()]
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
        List.duplicate("", length(windows))
    end
  end

  defp finalize([]), do: ""

  defp finalize(acc) do
    acc
    |> Enum.reverse()
    |> Enum.map_join("\n", &String.trim_trailing(&1, "\n"))
  end

  # --- Multi-window single-pass extraction ---

  # All windows consumed — finalize current accumulator and pad remaining with ""
  defp collect_multi_windows(_device, [], acc, results) do
    Enum.reverse([finalize(acc) | results])
  end

  defp collect_multi_windows(device, [{start_dt, end_dt} | rest_windows] = windows, acc, results) do
    case IO.read(device, :line) do
      :eof ->
        # Finalize current window and pad remaining windows with ""
        remaining = [finalize(acc) | results]
        padded = List.duplicate("", length(rest_windows))
        Enum.reverse(remaining) ++ padded

      {:error, _} ->
        remaining = [finalize(acc) | results]
        padded = List.duplicate("", length(rest_windows))
        Enum.reverse(remaining) ++ padded

      line ->
        case parse_timestamp(line) do
          {:ok, ts} ->
            cond do
              DateTime.compare(ts, start_dt) == :lt ->
                # Before current window — skip
                collect_multi_windows(device, windows, acc, results)

              DateTime.compare(ts, end_dt) == :gt ->
                # Past current window — finalize it, try this line against next window
                collect_multi_windows(
                  device,
                  rest_windows,
                  [],
                  [finalize(acc) | results],
                  line
                )

              true ->
                # Inside current window — collect
                collect_multi_windows(device, windows, [line | acc], results)
            end

          :error ->
            # Non-timestamped line: include only if inside a window (acc non-empty)
            if acc != [] do
              collect_multi_windows(device, windows, [line | acc], results)
            else
              collect_multi_windows(device, windows, acc, results)
            end
        end
    end
  end

  # Re-process a line that overshot the previous window against remaining windows
  defp collect_multi_windows(_device, [], _acc, results, _pending_line) do
    Enum.reverse(results)
  end

  defp collect_multi_windows(
         device,
         [{start_dt, end_dt} | rest_windows] = windows,
         acc,
         results,
         line
       ) do
    case parse_timestamp(line) do
      {:ok, ts} ->
        cond do
          DateTime.compare(ts, start_dt) == :lt ->
            collect_multi_windows(device, windows, acc, results)

          DateTime.compare(ts, end_dt) == :gt ->
            collect_multi_windows(device, rest_windows, [], [finalize(acc) | results], line)

          true ->
            collect_multi_windows(device, windows, [line | acc], results)
        end

      :error ->
        collect_multi_windows(device, windows, acc, results)
    end
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
