defmodule ToastTest.Enrichment.Sanitizer do
  @moduledoc """
  Read and classify sanitizer report files.

  Detects the sanitizer type from the filename and extracts the file's
  modification time as a timestamp for attribution.
  """

  @type result :: %{
          content: String.t(),
          timestamp: Toast.timestamp(),
          type: atom(),
          kind: String.t() | nil
        }

  @doc """
  Read a sanitizer log file and return its content, timestamp, type, and kind.

  The `kind` is extracted from the first warning/error line in the report
  (e.g., "data race", "heap-buffer-overflow", "use-after-free").
  """
  @spec read(Path.t()) :: {:ok, result()} | {:error, term()}
  def read(path) do
    with {:ok, timestamp} <- file_mtime(path),
         {:ok, content} <- File.read(path) do
      {:ok,
       %{
         content: content,
         timestamp: timestamp,
         type: detect_type(path),
         kind: detect_kind(content)
       }}
    end
  end

  # Get file mtime with microsecond precision via Linux stat(1).
  # %y gives human-readable mtime with nanoseconds: "2026-01-15 11:00:05.820000000 +0100"
  # Falls back to File.stat (second precision) if parsing fails.
  defp file_mtime(path) do
    case System.cmd("stat", ["-c", "%y", path], stderr_to_stdout: true) do
      {output, 0} -> parse_stat_mtime(output, path)
      _ -> file_mtime_fallback(path)
    end
  end

  defp parse_stat_mtime(output, path) do
    case Regex.run(
           ~r/^(\d{4}-\d{2}-\d{2}) (\d{2}:\d{2}:\d{2})\.(\d+) ([+-]\d{4})/,
           String.trim(output)
         ) do
      [_, date, time, nanos, offset] ->
        usec_str = nanos |> String.slice(0, 6) |> String.pad_trailing(6, "0")
        <<tz_h::binary-size(3), tz_m::binary>> = offset
        iso = "#{date}T#{time}.#{usec_str}#{tz_h}:#{tz_m}"

        case DateTime.from_iso8601(iso) do
          {:ok, dt, _offset} -> {:ok, DateTime.to_unix(dt, :microsecond)}
          _ -> file_mtime_fallback(path)
        end

      _ ->
        file_mtime_fallback(path)
    end
  end

  defp file_mtime_fallback(path) do
    case File.stat(path, time: :posix) do
      {:ok, stat} -> {:ok, stat.mtime * 1_000_000}
      error -> error
    end
  end

  @kind_pattern ~r/(?:WARNING|ERROR): \w+Sanitizer: (.+?)(?:\s*\(|$)/m

  defp detect_kind(content) do
    case Regex.run(@kind_pattern, content) do
      [_, kind] -> kind
      _ -> nil
    end
  end

  defp detect_type(path) do
    basename = Path.basename(path)

    cond do
      String.starts_with?(basename, "alubsan.log") -> :alubsan
      String.starts_with?(basename, "tsan.log") -> :tsan
      true -> :unknown
    end
  end
end
