defmodule Mix.Tasks.Toast.Analyze.Weights do
  @moduledoc false

  alias Mix.Tasks.Toast.Analyze.Data

  import ToastTest.Formatting, only: [colorize: 3, format_duration_us: 1]

  @doc """
  Suggests module weights based on runtime distribution from suite results.

  Takes a list of `%ToastTest.SuiteResult{}` structs and returns suggestions
  where the suggested weight differs from the current weight, sorted by
  duration descending.
  """
  @spec suggest_weights([ToastTest.SuiteResult.t()]) :: [map()]
  def suggest_weights([]), do: []

  def suggest_weights(results) do
    module_stats =
      for result <- results,
          {module, %{tests: tests}} <- result.modules do
        duration_us = tests |> Enum.map(& &1.duration_us) |> Enum.sum()
        current_weight = get_weight(module)

        %{
          module: Atom.to_string(module),
          suite: result.suite,
          duration_us: duration_us,
          current_weight: current_weight
        }
      end

    median = median_duration(module_stats)

    module_stats
    |> Enum.map(fn entry ->
      Map.put(entry, :suggested_weight, max(1, round(entry.duration_us / median)))
    end)
    |> Enum.filter(fn entry -> entry.suggested_weight != entry.current_weight end)
    |> Enum.sort_by(& &1.duration_us, :desc)
  end

  defp get_weight(module), do: ToastTest.Suite.weight(module)

  defp median_duration(entries) do
    durations = entries |> Enum.map(& &1.duration_us) |> Enum.sort()
    len = length(durations)
    mid = div(len, 2)

    if rem(len, 2) == 0 do
      (Enum.at(durations, mid - 1) + Enum.at(durations, mid)) / 2
    else
      Enum.at(durations, mid)
    end
  end

  def run(result_dir, opts, color) do
    results =
      result_dir
      |> Data.load_results()
      |> Data.maybe_filter_suite(opts[:suite])

    print_suggestions(suggest_weights(results), color)
  end

  defp print_suggestions([], _color) do
    Mix.shell().info("All module weights match their runtime distribution — no changes needed.")
  end

  defp print_suggestions(suggestions, color) do
    mod_width =
      suggestions
      |> Enum.map(&String.length(short_module(&1.module)))
      |> Enum.max(fn -> 6 end)
      |> max(6)

    suite_width =
      suggestions
      |> Enum.map(&String.length(&1.suite))
      |> Enum.max(fn -> 5 end)
      |> max(5)

    header =
      " " <>
        String.pad_trailing("Module", mod_width) <>
        "  " <>
        String.pad_trailing("Suite", suite_width) <>
        "  Duration  Weight"

    Mix.shell().info("")
    Mix.shell().info(colorize(header, :cyan, color))

    Enum.each(suggestions, fn s ->
      row =
        " " <>
          String.pad_trailing(short_module(s.module), mod_width) <>
          "  " <>
          String.pad_trailing(s.suite, suite_width) <>
          "  " <>
          String.pad_leading(format_duration_us(s.duration_us), 8) <>
          "  " <>
          colorize("#{s.current_weight} -> #{s.suggested_weight}", :yellow, color)

      Mix.shell().info(row)
    end)

    Mix.shell().info("")

    Mix.shell().info(
      "Hint: set weight via " <>
        colorize("use YourSuite, weight: N", :bright, color) <>
        " in the test module."
    )
  end

  defp short_module("Elixir." <> rest), do: rest
  defp short_module(mod), do: mod
end
