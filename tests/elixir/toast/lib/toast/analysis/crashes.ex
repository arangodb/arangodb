defmodule Toast.Analysis.Crashes do
  @moduledoc "Format crash diagnostics for terminal output."

  @doc "Format crash and sanitizer information from parsed results.json data."
  @spec format(map()) :: String.t()
  def format(results) do
    sections = []

    sections = sections ++ format_crash_matching(results["crash_matching"])
    sections = sections ++ format_sanitizer_matching(results["sanitizer_matching"])
    sections = sections ++ format_server_health_crashes(results)
    sections = sections ++ format_coredump_reports(results)

    if sections == [] do
      "No crash or sanitizer issues detected."
    else
      Enum.join(sections, "\n\n")
    end
  end

  defp format_crash_matching(nil), do: []
  defp format_crash_matching(%{"matched" => [], "unmatched" => []}), do: []

  defp format_crash_matching(%{"matched" => matched, "unmatched" => unmatched}) do
    parts = []

    parts =
      if matched != [] do
        entries =
          Enum.map_join(matched, "\n", fn m ->
            "  #{m["module"]} - #{m["test"]} (#{m["confidence"]}): #{format_crash_brief(m["crash"])}"
          end)

        parts ++ ["Crash Attribution:\n" <> entries]
      else
        parts
      end

    parts =
      if unmatched != [] do
        entries =
          Enum.map_join(unmatched, "\n", fn c ->
            "  #{format_crash_brief(c)}"
          end)

        parts ++ ["Unattributed Crashes:\n" <> entries]
      else
        parts
      end

    parts
  end

  defp format_crash_matching(_), do: []

  defp format_crash_brief(nil), do: "(no details)"

  defp format_crash_brief(crash) when is_map(crash) do
    signal = crash["signal_name"] || "unknown signal"
    server = crash["server_id"] || "unknown"
    "[#{signal}] #{server}"
  end

  defp format_sanitizer_matching(nil), do: []
  defp format_sanitizer_matching(%{"matched" => [], "unmatched" => []}), do: []

  defp format_sanitizer_matching(%{"matched" => matched, "unmatched" => unmatched}) do
    parts = []

    parts =
      if matched != [] do
        entries =
          Enum.map_join(matched, "\n", fn m ->
            error = m["error"] || %{}
            type = String.upcase(error["sanitizer_type"] || "unknown")
            "  #{m["module"]} - #{m["test"]} (#{m["confidence"]}): [#{type}] #{error["server_id"]}"
          end)

        parts ++ ["Sanitizer Attribution:\n" <> entries]
      else
        parts
      end

    parts =
      if unmatched != [] do
        entries =
          Enum.map_join(unmatched, "\n", fn e ->
            type = String.upcase(e["sanitizer_type"] || "unknown")
            "  [#{type}] #{e["server_id"]} - #{e["file_path"]}"
          end)

        parts ++ ["Unattributed Sanitizer Errors:\n" <> entries]
      else
        parts
      end

    parts
  end

  defp format_sanitizer_matching(_), do: []

  defp format_server_health_crashes(results) do
    case results["server_health"] do
      nil -> []
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
      _ -> []
    end
  end

  defp collect_crash_reports(health) when is_map(health) do
    case health["crash_report"] do
      nil ->
        Enum.flat_map(health, fn
          {server_id, server_health} when is_map(server_health) ->
            case server_health["crash_report"] do
              nil -> []
              report -> ["  #{server_id}: #{report["signal_name"]} (signal #{report["signal_number"]})"]
            end
          _ -> []
        end)

      report ->
        ["  #{report["signal_name"]} (signal #{report["signal_number"]})"]
    end
  end

  defp format_coredump_reports(results) do
    suites = results["suites"] || []

    reports =
      Enum.flat_map(suites, fn suite ->
        case suite["diagnostics"] do
          %{"coredump_reports" => reports} when is_list(reports) and reports != [] ->
            Enum.map(reports, fn r ->
              "  #{r["core_path"]}: #{r["signal"]} (#{r["debugger"]})"
            end)
          _ -> []
        end
      end)

    if reports == [] do
      []
    else
      ["Coredump Reports:\n" <> Enum.join(reports, "\n")]
    end
  end
end
