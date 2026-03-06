defmodule Toast.Diagnostics.LogAnalyzer do
  @moduledoc """
  Single-pass analyzer for arangod server logs.

  Merges crash log parsing and server log scanning into one pass over the log
  content, producing a unified report with crash analysis, assertion failures,
  and non-trivial log warnings.
  """

  @type log_entry :: %{timestamp: DateTime.t() | nil, message: String.t()}

  alias Toast.Diagnostics.LogReport

  @type log_report :: LogReport.t()

  # Log topic IDs that produce uninteresting noise (arangod internal topics)
  @uninteresting_topics ["de8f3", "e8b68", "1afb1", "d72fb", "f3108"]

  # Startup phase markers — log messages between these IDs are routine startup
  # noise and should be ignored for test attribution.
  @startup_begin_id "e52b0"
  @startup_end_id "3bb7d"

  @doc "Parse a log file. Returns nil if file doesn't exist."
  @spec parse(Path.t() | nil) :: log_report() | nil
  def parse(nil), do: nil

  def parse(path) do
    if File.exists?(path) do
      path |> File.stream!() |> parse_stream()
    end
  end

  @doc "Parse log lines from any enumerable (e.g., a stream or list of strings)."
  @spec parse_stream(Enumerable.t()) :: log_report()
  def parse_stream(lines) do
    lines
    |> Enum.reduce(new(), &process_line/2)
    |> finalize()
  end

  @spec format_summary(LogReport.t()) :: String.t()
  def format_summary(%LogReport{signal_name: nil}), do: "No crash detected"

  def format_summary(%LogReport{signal_name: name, signal_number: number, backtrace: bt}) do
    "#{name} (signal #{number}) - #{length(bt)} backtrace frames"
  end

  defp new do
    %{
      signal_number: nil,
      signal_name: nil,
      crash_header: nil,
      backtrace: [],
      fatal_lines: [],
      crash_output: [],
      timestamp: nil,
      assertion_failures: [],
      warnings: [],
      uninteresting_topics: @uninteresting_topics,
      in_startup_phase: false
    }
  end

  defp process_line(line, report) do
    report
    |> track_startup_phase(line)
    |> maybe_collect_fatal(line)
    |> maybe_collect_crash(line)
    |> maybe_collect_assertion(line)
    |> maybe_collect_warning(line)
  end

  defp track_startup_phase(report, line) do
    cond do
      String.contains?(line, "[#{@startup_begin_id}]") -> %{report | in_startup_phase: true}
      String.contains?(line, "[#{@startup_end_id}]") -> %{report | in_startup_phase: false}
      true -> report
    end
  end

  defp finalize(report) do
    %LogReport{
      signal_number: report.signal_number,
      signal_name: report.signal_name,
      crash_header: report.crash_header,
      backtrace: Enum.reverse(report.backtrace),
      fatal_lines: Enum.reverse(report.fatal_lines),
      crash_output: Enum.reverse(report.crash_output),
      timestamp: report.timestamp,
      assertion_failures: Enum.reverse(report.assertion_failures),
      warnings: Enum.reverse(report.warnings)
    }
  end

  # --- Fatal lines (FATAL non-crash, bare strings for crash diagnostic display) ---

  defp maybe_collect_fatal(report, line) do
    if fatal_line?(line) and not String.contains?(line, "{crash}") do
      content = extract_log_message(line)
      %{report | fatal_lines: [content | report.fatal_lines]}
    else
      report
    end
  end

  # --- Crash analysis ---

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

  # --- Assertion failures ---

  defp maybe_collect_assertion(report, line) do
    if String.contains?(line, "{assertion}") do
      entry = %{
        timestamp: extract_timestamp(line),
        message: extract_log_message(line)
      }

      %{report | assertion_failures: [entry | report.assertion_failures]}
    else
      report
    end
  end

  # --- Broadened warning capture (all non-INFO, non-crash, non-trivial lines) ---

  defp maybe_collect_warning(report, line) do
    cond do
      report.in_startup_phase ->
        report

      info_line?(line) ->
        report

      String.contains?(line, "{crash}") ->
        report

      String.contains?(line, "{assertion}") ->
        report

      uninteresting_topic?(line, report.uninteresting_topics) ->
        report

      String.contains?(line, "WARNING about to execute:") ->
        report

      not log_line?(line) ->
        report

      true ->
        entry = %{
          timestamp: extract_timestamp(line),
          message: extract_log_message(line)
        }

        %{report | warnings: [entry | report.warnings]}
    end
  end

  defp info_line?(line), do: String.contains?(line, " INFO ")

  defp log_line?(line) do
    Regex.match?(~r/^\s*\d{4}-\d{2}-\d{2}T[\d:.]+/, line)
  end

  defp uninteresting_topic?(line, topics) do
    Enum.any?(topics, fn topic -> String.contains?(line, "[#{topic}]") end)
  end

  # --- Public helpers ---

  @doc """
  Check if a log line has FATAL severity.

  Matches both old and new arangod log formats:
  - older: `[pid] FATAL [logid]`
  - newer: `[pid-tid] R FATAL [logid]` (R = server role)
  """
  @spec fatal_line?(String.t()) :: boolean()
  def fatal_line?(line), do: String.contains?(line, " FATAL ")

  @doc "Extract message content after the log topic marker (e.g., `{general} `, `{crash} `)."
  @spec extract_log_message(String.t()) :: String.t()
  def extract_log_message(line) do
    case Regex.run(~r/\{[^}]+\}\s+(.*)$/, line) do
      [_, content] -> content
      _ -> line
    end
  end

  # --- Private helpers ---

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
