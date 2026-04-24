defmodule ToastTest.Enrichment.Sanitizer do
  @moduledoc """
  Read and classify sanitizer report files.

  A single sanitizer log file (e.g. `tsan.log.arangod.12345`) may contain
  multiple reports.  `read_all/2` splits the file on report boundaries and
  optionally pairs each report with a per-report timestamp from a sidecar
  file written by the `__sanitizer_report_error_summary` hook (see
  `arangod/RestServer/SanitizerReportHook.cpp`).

  When no sidecar is available or the sidecar entry count does not match
  the number of reports, timestamps are `nil` and the reports are treated
  as non-attributable.
  """

  @type result :: %{
          content: String.t(),
          timestamp: Toast.timestamp() | nil,
          type: atom(),
          kind: String.t() | nil
        }

  @doc """
  Read a sanitizer log file and return one result per report.

  When `sidecar_path` points to a valid sidecar file whose entry count
  matches the number of reports, per-report timestamps from the sidecar
  are used.  Otherwise timestamps are `nil`.
  """
  @spec read_all(Path.t(), Path.t() | nil) :: {:ok, [result()]} | {:error, term()}
  def read_all(path, sidecar_path \\ nil) do
    with {:ok, content} <- File.read(path) do
      type = detect_type(path)
      reports = split_reports(content)
      timestamps = resolve_timestamps(reports, sidecar_path)

      results =
        Enum.zip_with(reports, timestamps, fn report, ts ->
          %{content: report, timestamp: ts, type: type, kind: detect_kind(report)}
        end)

      {:ok, results}
    end
  end

  @doc """
  Derive the sidecar path for a given sanitizer file.

  Extracts the PID (last `.`-delimited segment of the filename) and builds
  the sidecar path using the `sanitizer_reports.log` prefix convention.
  Returns `nil` if the PID cannot be extracted.
  """
  @spec sidecar_path_for(Path.t()) :: Path.t() | nil
  def sidecar_path_for(sanitizer_file) do
    dir = Path.dirname(sanitizer_file)
    prefix = Toast.Diagnostics.Sanitizer.report_log_prefix(dir)

    case extract_pid(Path.basename(sanitizer_file)) do
      nil -> nil
      pid -> "#{prefix}.#{pid}"
    end
  end

  defp extract_pid(basename) do
    case String.split(basename, ".") |> List.last() do
      "" -> nil
      pid -> if String.match?(pid, ~r/^\d+$/), do: pid
    end
  end

  # --- Report splitting ---

  @delimiter "=================="

  defp split_reports(content) do
    content
    |> String.split(@delimiter)
    |> Enum.map(&String.trim/1)
    |> Enum.reject(&(&1 == ""))
  end

  # --- Timestamp resolution ---

  defp resolve_timestamps(reports, sidecar_path) do
    count = length(reports)

    case read_sidecar(sidecar_path) do
      timestamps when length(timestamps) == count -> timestamps
      _ -> List.duplicate(nil, count)
    end
  end

  defp read_sidecar(nil), do: nil

  defp read_sidecar(path) do
    case File.read(path) do
      {:ok, content} -> parse_sidecar(content)
      {:error, _} -> nil
    end
  end

  defp parse_sidecar(content) do
    content
    |> String.split("\n", trim: true)
    |> Enum.map(fn line ->
      case String.split(line, "\t", parts: 2) do
        [ts_str, _summary] -> String.to_integer(ts_str)
        _ -> nil
      end
    end)
    |> then(fn entries ->
      if Enum.any?(entries, &is_nil/1), do: nil, else: entries
    end)
  end

  # --- Classification ---

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
