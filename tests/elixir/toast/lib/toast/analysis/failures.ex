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
    header = format_failure_header(test, idx)
    failure_lines = format_failure_detail(test["failure"])
    Enum.join([header | failure_lines], "\n")
  end

  defp format_failure_header(test, idx) do
    module = test["module"] || "Unknown"
    name = test["name"] || "unknown"

    header = "#{idx}) #{module} - #{name}"
    header = if test["suite_name"], do: header <> " [#{test["suite_name"]}]", else: header

    case test["duration_seconds"] do
      nil -> header
      duration -> header <> " (#{Float.round(duration + 0.0, 3)}s)"
    end
  end

  defp format_failure_detail(nil), do: []
  defp format_failure_detail(f) when is_map(f), do: ["   #{f["message"] || "no message"}"]
  defp format_failure_detail(fs) when is_list(fs), do: Enum.flat_map(fs, &format_single_failure/1)
  defp format_failure_detail(_), do: []

  defp format_single_failure(f) do
    msg = f["message"] || "no message"

    [
      "   [#{f["kind"]}] #{msg}",
      if(f["stacktrace"], do: "   #{truncate(f["stacktrace"], 500)}")
    ]
    |> Enum.reject(&is_nil/1)
  end

  defp truncate(text, max_len) when is_binary(text) and byte_size(text) > max_len do
    String.slice(text, 0, max_len) <> "..."
  end

  defp truncate(text, _max_len) when is_binary(text), do: text
  defp truncate(other, _), do: inspect(other)
end
