defmodule Toast.Analysis.Performance do
  @moduledoc "Format performance analysis for terminal output."

  @doc "Format slowest tests. n defaults to 10."
  @spec format(map(), non_neg_integer()) :: String.t()
  def format(results, n \\ 10) do
    modules = results["modules"] || %{}

    all_tests =
      Enum.flat_map(modules, fn {_module_name, mod} ->
        mod["tests"] || []
      end)

    slowest =
      all_tests
      |> Enum.sort_by(&(-(&1["duration_seconds"] || 0)))
      |> Enum.take(n)

    lines = [format_slowest(slowest, n), "", format_distribution(all_tests)]
    Enum.join(lines, "\n")
  end

  defp format_slowest([], _n), do: "No tests found."

  defp format_slowest(tests, n) do
    header = "Slowest #{min(n, length(tests))} tests:\n"

    entries =
      tests
      |> Enum.with_index(1)
      |> Enum.map_join("\n", fn {test, idx} ->
        module = test["module"] || "Unknown"
        name = test["name"] || "unknown"
        duration = test["duration_seconds"] || 0
        "  #{idx}. #{module} - #{name} (#{Float.round(duration + 0.0, 3)}s)"
      end)

    header <> entries
  end

  defp format_distribution(tests) do
    buckets = %{
      "<1s" => 0,
      "1-5s" => 0,
      "5-30s" => 0,
      ">30s" => 0
    }

    buckets =
      Enum.reduce(tests, buckets, fn test, acc ->
        duration = test["duration_seconds"] || 0

        key =
          cond do
            duration < 1 -> "<1s"
            duration < 5 -> "1-5s"
            duration < 30 -> "5-30s"
            true -> ">30s"
          end

        Map.update!(acc, key, &(&1 + 1))
      end)

    lines =
      ["<1s", "1-5s", "5-30s", ">30s"]
      |> Enum.map(fn key -> "  #{key}: #{buckets[key]} tests" end)

    "Duration distribution:\n" <> Enum.join(lines, "\n")
  end
end
