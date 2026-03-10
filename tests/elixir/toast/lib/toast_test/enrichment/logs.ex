defmodule ToastTest.Enrichment.Logs do
  @moduledoc """
  Extract time-windowed excerpts from ArangoDB log files.

  Parses only the leading timestamp from each line to filter efficiently.
  Assumes log lines are chronologically ordered — skips lines before the
  window and stops after the window ends.
  """

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
      :eof ->
        finalize(acc)

      {:error, _} ->
        finalize(acc)

      line ->
        case parse_timestamp(line) do
          {:ok, ts} ->
            cond do
              DateTime.compare(ts, start_dt) == :lt ->
                collect_lines(device, start_dt, end_dt, acc)

              DateTime.compare(ts, end_dt) == :gt ->
                finalize(acc)

              true ->
                collect_lines(device, start_dt, end_dt, [line | acc])
            end

          :error ->
            # Non-timestamped lines: include if we're already inside the window
            if acc != [] do
              collect_lines(device, start_dt, end_dt, [line | acc])
            else
              collect_lines(device, start_dt, end_dt, acc)
            end
        end
    end
  end

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
