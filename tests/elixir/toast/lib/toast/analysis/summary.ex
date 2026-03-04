defmodule Toast.Analysis.Summary do
  @moduledoc "Format test result summary for terminal output."

  @doc "Format a summary from parsed results.json data."
  @spec format(map()) :: String.t()
  def format(results) do
    summary = results["summary"]
    modules = results["modules"] || %{}

    lines = [
      format_totals(summary),
      format_duration(results),
      format_module_breakdown(modules)
    ]

    Enum.join(lines, "\n")
  end

  defp format_totals(summary) do
    total = summary["total"] || 0
    passed = summary["passed"] || 0
    failed = summary["failed"] || 0
    skipped = summary["skipped"] || 0

    "Total: #{total} tests, #{passed} passed, #{failed} failed, #{skipped} skipped"
  end

  defp format_duration(results) do
    seconds = get_in(results, ["test_run", "duration_seconds"]) || 0
    "Duration: #{format_time(seconds)}"
  end

  defp format_module_breakdown(modules) when map_size(modules) == 0, do: ""

  defp format_module_breakdown(modules) do
    lines =
      Enum.map(modules, fn {name, mod} ->
        tests = mod["tests"] || []
        passed = Enum.count(tests, &(&1["outcome"] == "passed"))
        failed = Enum.count(tests, &(&1["outcome"] == "failed"))
        duration = mod["duration_seconds"] || 0
        "  #{name}: #{passed} passed, #{failed} failed (#{format_time(duration)})"
      end)

    "Module breakdown:\n" <> Enum.join(lines, "\n")
  end

  defp format_time(seconds) when is_number(seconds) do
    minutes = trunc(seconds / 60)
    secs = seconds - minutes * 60

    if minutes > 0 do
      "#{minutes}m #{Float.round(secs + 0.0, 1)}s"
    else
      "#{Float.round(secs + 0.0, 1)}s"
    end
  end
end
