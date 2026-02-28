defmodule Toast.Diagnostics.ServerLog do
  @moduledoc "Scan arangod server logs for non-crash issues."

  alias Toast.Diagnostics.CrashLogParser

  @type server_log_report :: %{
          assertion_failures: [String.t()],
          warnings: [String.t()]
        }

  @doc """
  Scan log content for important non-crash messages.

  Extracts:
  - Lines with `{assertion}` topic -> assertion_failures
  - FATAL lines NOT in `{crash}` topic -> warnings
  """
  @spec scan(String.t()) :: server_log_report()
  def scan(content) do
    report =
      content
      |> String.split("\n")
      |> Enum.reduce(empty_report(), fn line, report ->
        report
        |> maybe_collect_assertion(line)
        |> maybe_collect_warning(line)
      end)

    %{
      report
      | assertion_failures: Enum.reverse(report.assertion_failures),
        warnings: Enum.reverse(report.warnings)
    }
  end

  defp empty_report do
    %{assertion_failures: [], warnings: []}
  end

  defp maybe_collect_assertion(report, line) do
    if String.contains?(line, "{assertion}") do
      content = extract_message(line)
      %{report | assertion_failures: [content | report.assertion_failures]}
    else
      report
    end
  end

  defp maybe_collect_warning(report, line) do
    if CrashLogParser.fatal_line?(line) and not String.contains?(line, "{crash}") do
      content = extract_message(line)
      %{report | warnings: [content | report.warnings]}
    else
      report
    end
  end

  defp extract_message(line) do
    # Extract content after the log topic (e.g., "{general} message here")
    case Regex.run(~r/\{[^}]+\}\s+(.*)$/, line) do
      [_, content] -> content
      _ -> line
    end
  end
end
