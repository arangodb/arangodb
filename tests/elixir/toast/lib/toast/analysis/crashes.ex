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
    matched_section =
      if matched != [] do
        entries = Enum.map_join(matched, "\n", format_matched_fn)
        [matched_header <> "\n" <> entries]
      else
        []
      end

    unmatched_section =
      if unmatched != [] do
        entries = Enum.map_join(unmatched, "\n", format_unmatched_fn)
        [unmatched_header <> "\n" <> entries]
      else
        []
      end

    matched_section ++ unmatched_section
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
      nil ->
        []

      health when is_map(health) ->
        crash_entries =
          health
          |> collect_crash_reports()
          |> Enum.reject(&is_nil/1)

        if crash_entries == [] do
          []
        else
          ["Server Crashes:\n" <> Enum.join(crash_entries, "\n")]
        end

      _ ->
        []
    end
  end

  defp collect_crash_reports(health) when is_map(health) do
    Enum.flat_map(health, &format_server_crash_report/1)
  end

  defp format_server_crash_report({server_id, server_health}) when is_map(server_health) do
    case server_health["crash_report"] do
      nil -> []
      report -> ["  #{server_id}: #{report["signal_name"]} (signal #{report["signal_number"]})"]
    end
  end

  defp format_server_crash_report(_), do: []

  defp format_coredump_reports(results) do
    suites = results["suites"] || []

    reports =
      Enum.flat_map(suites, &extract_coredump_lines/1)

    if reports == [] do
      []
    else
      ["Coredump Reports:\n" <> Enum.join(reports, "\n")]
    end
  end

  defp extract_coredump_lines(suite) do
    case suite["diagnostics"] do
      %{"coredump_reports" => reports} when is_list(reports) and reports != [] ->
        Enum.map(reports, fn r ->
          "  #{r["core_path"]}: #{r["signal"]} (#{r["debugger"]})"
        end)

      _ ->
        []
    end
  end
end
