defmodule Toast.Diagnostics.CrashLogParser do
  @moduledoc "Parse arangod log files for crash diagnostics."

  @type crash_report :: %{
          signal_number: non_neg_integer() | nil,
          signal_name: String.t() | nil,
          crash_header: String.t() | nil,
          backtrace: [String.t()],
          fatal_lines: [String.t()],
          crash_output: [String.t()],
          timestamp: DateTime.t() | nil
        }

  @doc "Parse log content from a string. Prefer `parse_file/1` for large logs."
  @spec parse(String.t()) :: crash_report()
  def parse(content) do
    content
    |> String.split("\n")
    |> Enum.reduce(new(), &process_line/2)
    |> finalize()
  end

  @doc "Stream a log file and parse crash diagnostics without loading it into memory."
  @spec parse_file(Path.t()) :: crash_report() | nil
  def parse_file(path) do
    if File.exists?(path) do
      path
      |> File.stream!()
      |> Enum.reduce(new(), &process_line/2)
      |> finalize()
    end
  end

  @spec has_crash?(String.t()) :: boolean()
  def has_crash?(content), do: String.contains?(content, "{crash}")

  @spec format_summary(crash_report()) :: String.t()
  def format_summary(%{signal_name: nil}), do: "No crash detected"

  def format_summary(%{signal_name: name, signal_number: number, backtrace: bt}) do
    "#{name} (signal #{number}) - #{length(bt)} backtrace frames"
  end

  @doc "Create initial accumulator for streaming line-by-line processing."
  @spec new() :: map()
  def new do
    %{
      signal_number: nil,
      signal_name: nil,
      crash_header: nil,
      backtrace: [],
      fatal_lines: [],
      crash_output: [],
      timestamp: nil
    }
  end

  @doc "Process a single log line, updating the accumulator."
  @spec process_line(String.t(), map()) :: map()
  def process_line(line, report) do
    report
    |> maybe_collect_fatal(line)
    |> maybe_collect_crash(line)
  end

  @doc "Finalize the accumulator into a crash_report (reverses collected lists)."
  @spec finalize(map()) :: crash_report()
  def finalize(report) do
    %{
      report
      | fatal_lines: Enum.reverse(report.fatal_lines),
        backtrace: Enum.reverse(report.backtrace),
        crash_output: Enum.reverse(report.crash_output)
    }
  end

  defp maybe_collect_fatal(report, line) do
    # Collect FATAL lines that are NOT in {crash} topic — crash-specific fatals
    # are handled separately as crash_header / backtrace.
    if fatal_line?(line) and not String.contains?(line, "{crash}") do
      content = extract_after_prefix(line)
      %{report | fatal_lines: [content | report.fatal_lines]}
    else
      report
    end
  end

  defp maybe_collect_crash(report, line) do
    if String.contains?(line, "{crash}") do
      crash_content = extract_crash_content(line)

      report
      |> maybe_set_crash_header(line, crash_content)
      |> maybe_collect_backtrace_frame(crash_content)
      |> Map.update!(:crash_output, &[crash_content | &1])
    else
      report
    end
  end

  defp maybe_set_crash_header(report, line, crash_content) do
    if report.crash_header == nil and fatal_line?(line) do
      {signal_number, signal_name} = extract_signal(crash_content)
      timestamp = extract_timestamp(line)

      %{
        report
        | crash_header: crash_content,
          signal_number: signal_number,
          signal_name: signal_name,
          timestamp: timestamp
      }
    else
      report
    end
  end

  defp maybe_collect_backtrace_frame(report, crash_content) do
    if Regex.match?(~r/frame\s+\d+/, crash_content) do
      %{report | backtrace: [crash_content | report.backtrace]}
    else
      report
    end
  end

  @doc """
  Check if a log line has FATAL severity.

  Matches both old and new arangod log formats:
  - older: `[pid] FATAL [logid]`
  - newer: `[pid-tid] R FATAL [logid]` (R = server role)
  """
  @spec fatal_line?(String.t()) :: boolean()
  def fatal_line?(line), do: String.contains?(line, " FATAL ")

  defp extract_signal(text) do
    case Regex.run(~r/caught unexpected signal (\d+) \((\w+)/, text) do
      [_, number, name] -> {String.to_integer(number), name}
      _ -> {nil, nil}
    end
  end

  defp extract_crash_content(line) do
    case String.split(line, "{crash} ", parts: 2) do
      [_, content] -> content
      _ -> line
    end
  end

  defp extract_after_prefix(line) do
    # Extract content after the log topic (e.g., "{general} ", "{crash} ")
    case Regex.run(~r/\{[^}]+\}\s+(.*)$/, line) do
      [_, content] -> content
      _ -> line
    end
  end

  defp extract_timestamp(line) do
    case Regex.run(
           ~r/^(\d{4}-\d{2}-\d{2}T[\d:.]+(?:Z|[+-]\d{2}:\d{2})?)/,
           String.trim_leading(line)
         ) do
      [_, ts_string] -> parse_timestamp(ts_string)
      _ -> nil
    end
  end

  defp parse_timestamp(ts_string) do
    case DateTime.from_iso8601(ts_string) do
      {:ok, dt, _offset} ->
        dt

      {:error, :missing_offset} ->
        case NaiveDateTime.from_iso8601(ts_string) do
          {:ok, ndt} -> DateTime.from_naive!(ndt, "Etc/UTC")
          _ -> nil
        end

      _ ->
        nil
    end
  end
end
