defmodule Toast.Analysis.Crashes do
  @moduledoc "Format crash diagnostics for terminal output."

  @doc "Format crash and sanitizer information from parsed results.json data."
  @spec format(map()) :: String.t()
  def format(results) do
    sections =
      format_matching(
        results["crash_matching"],
        "Crash Attribution:",
        "Unattributed Crashes:",
        &format_matched_crash/1,
        &format_unmatched_crash/1
      ) ++
        format_matching(
          results["sanitizer_matching"],
          "Sanitizer Attribution:",
          "Unattributed Sanitizer Errors:",
          &format_matched_sanitizer/1,
          &format_unmatched_sanitizer/1
        ) ++
        format_server_health_crashes(results) ++
        format_coredump_reports(results)

    if sections == [] do
      "No crash or sanitizer issues detected."
    else
      Enum.join(sections, "\n\n")
    end
  end

  # Generic matched/unmatched formatter — single place for this structural pattern.
  defp format_matching(nil, _, _, _, _), do: []
  defp format_matching(%{"matched" => [], "unmatched" => []}, _, _, _, _), do: []

  defp format_matching(
         %{"matched" => matched, "unmatched" => unmatched},
         matched_header,
         unmatched_header,
         format_matched_fn,
         format_unmatched_fn
       ) do
    format_section(matched, matched_header, format_matched_fn) ++
      format_section(unmatched, unmatched_header, format_unmatched_fn)
  end

  defp format_matching(_, _, _, _, _), do: []

  defp format_matched_crash(m) do
    "  #{m["module"]} - #{m["test"]} (#{m["confidence"]}): #{format_crash_brief(m["crash"])}"
  end

  defp format_unmatched_crash(c), do: "  #{format_crash_brief(c)}"

  defp format_crash_brief(nil), do: "(no details)"

  defp format_crash_brief(crash) when is_map(crash) do
    signal = crash["signal_name"] || "unknown signal"
    server = crash["server_id"] || "unknown"
    "[#{signal}] #{server}"
  end

  defp format_matched_sanitizer(m) do
    error = m["error"] || %{}
    type = String.upcase(error["sanitizer_type"] || "unknown")
    "  #{m["module"]} - #{m["test"]} (#{m["confidence"]}): [#{type}] #{error["server_id"]}"
  end

  defp format_unmatched_sanitizer(e) do
    type = String.upcase(e["sanitizer_type"] || "unknown")
    "  [#{type}] #{e["server_id"]} - #{e["file_path"]}"
  end

  defp format_server_health_crashes(results) do
    case results["server_health"] do
      health when is_map(health) -> format_section(collect_crash_reports(health), "Server Crashes:")
      _ -> []
    end
  end

  defp collect_crash_reports(health) when is_map(health) do
    Enum.flat_map(health, &format_server_crash_report/1)
  end

  defp format_server_crash_report({server_id, %{"crash_report" => report}}) when is_map(report) do
    ["  #{server_id}: #{report["signal_name"]} (signal #{report["signal_number"]})"]
  end

  defp format_server_crash_report(_), do: []

  defp format_coredump_reports(results) do
    results["suites"]
    |> List.wrap()
    |> Enum.flat_map(&extract_coredump_lines/1)
    |> format_section("Coredump Reports:")
  end

  defp format_section([], _header), do: []
  defp format_section(entries, header), do: [header <> "\n" <> Enum.join(entries, "\n")]

  defp format_section([], _header, _formatter), do: []

  defp format_section(items, header, formatter),
    do: [header <> "\n" <> Enum.map_join(items, "\n", formatter)]

  defp extract_coredump_lines(suite) do
    suite
    |> get_in(["diagnostics", "coredump_reports"])
    |> List.wrap()
    |> Enum.map(fn r -> "  #{r["core_path"]}: #{r["signal"]} (#{r["debugger"]})" end)
  end
end
