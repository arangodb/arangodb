defmodule Toast.Diagnostics.CrashLogParser do
  @moduledoc "Parse arangod log files for crash diagnostics."

  @type crash_report :: %{
          signal_number: non_neg_integer() | nil,
          signal_name: String.t() | nil,
          crash_header: String.t() | nil,
          backtrace: [String.t()],
          fatal_lines: [String.t()]
        }

  @spec parse(String.t()) :: crash_report()
  def parse(content) do
    lines = String.split(content, "\n")

    report =
      Enum.reduce(lines, empty_report(), fn line, report ->
        report
        |> maybe_collect_fatal(line)
        |> maybe_collect_crash(line)
      end)

    %{report | fatal_lines: Enum.reverse(report.fatal_lines), backtrace: Enum.reverse(report.backtrace)}
  end

  @spec has_crash?(String.t()) :: boolean()
  def has_crash?(content), do: String.contains?(content, "{crash}")

  @spec format_summary(crash_report()) :: String.t()
  def format_summary(%{signal_name: nil}), do: "No crash detected"

  def format_summary(%{signal_name: name, signal_number: number, backtrace: bt}) do
    "#{name} (signal #{number}) - #{length(bt)} backtrace frames"
  end

  defp empty_report do
    %{
      signal_number: nil,
      signal_name: nil,
      crash_header: nil,
      backtrace: [],
      fatal_lines: []
    }
  end

  defp maybe_collect_fatal(report, line) do
    if String.contains?(line, "] FATAL [") do
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
    else
      report
    end
  end

  defp maybe_set_crash_header(report, line, crash_content) do
    if report.crash_header == nil and String.contains?(line, "] FATAL [") do
      {signal_number, signal_name} = extract_signal(crash_content)

      %{report | crash_header: crash_content, signal_number: signal_number, signal_name: signal_name}
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
end
