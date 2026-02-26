defmodule Toast.Analysis.Failures do
  @moduledoc "Format detailed failure information for terminal output."

  @doc "Format failure details from parsed results.json data."
  @spec format(map()) :: String.t()
  def format(results) do
    suites = results["suites"] || []

    failed_tests =
      Enum.flat_map(suites, fn suite ->
        tests = suite["tests"] || []
        suite_name = suite["name"]

        tests
        |> Enum.filter(&(&1["outcome"] == "failed"))
        |> Enum.map(&Map.put(&1, "suite_name", suite_name))
      end)

    if failed_tests == [] do
      "No failures."
    else
      header = "#{length(failed_tests)} failure(s):\n"

      details =
        failed_tests
        |> Enum.with_index(1)
        |> Enum.map_join("\n\n", fn {test, idx} ->
          format_failure(test, idx)
        end)

      header <> details
    end
  end

  defp format_failure(test, idx) do
    module = test["module"] || "Unknown"
    name = test["name"] || "unknown"
    suite = test["suite_name"]
    duration = test["duration_seconds"]

    header = "#{idx}) #{module} - #{name}"
    header = if suite, do: header <> " [#{suite}]", else: header
    header = if duration, do: header <> " (#{Float.round(duration + 0.0, 3)}s)", else: header

    lines = [header]

    lines =
      case test["failure"] do
        nil -> lines
        failure when is_map(failure) ->
          lines ++ ["   #{failure["message"] || "no message"}"]
        failures when is_list(failures) ->
          Enum.reduce(failures, lines, fn f, acc ->
            msg = f["message"] || "no message"
            kind = f["kind"]
            st = f["stacktrace"]
            entry = ["   [#{kind}] #{msg}"]
            entry = if st, do: entry ++ ["   #{truncate(st, 500)}"], else: entry
            acc ++ entry
          end)
        _ -> lines
      end

    Enum.join(lines, "\n")
  end

  defp truncate(text, max_len) when is_binary(text) and byte_size(text) > max_len do
    String.slice(text, 0, max_len) <> "..."
  end

  defp truncate(text, _max_len) when is_binary(text), do: text
  defp truncate(other, _), do: inspect(other)
end
