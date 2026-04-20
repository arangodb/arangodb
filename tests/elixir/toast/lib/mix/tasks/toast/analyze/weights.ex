defmodule Mix.Tasks.Toast.Analyze.Weights do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3, format_duration_us: 1]

  @doc """
  Suggests module weights based on runtime distribution from outcomes data.

  Takes a map of `%{suite_name => [%{"module" => ..., "duration_us" => ..., "weight" => ...}]}`
  and returns suggestions where the suggested weight differs from the current weight,
  sorted by duration descending.
  """
  @spec suggest_weights(%{String.t() => [map()]}) :: [map()]
  def suggest_weights(outcomes_by_suite) when outcomes_by_suite == %{}, do: []

  def suggest_weights(outcomes_by_suite) do
    module_stats =
      for {suite, tests} <- outcomes_by_suite,
          {module, duration_us, current_weight} <- aggregate_by_module(tests) do
        %{module: module, suite: suite, duration_us: duration_us, current_weight: current_weight}
      end

    median = median_duration(module_stats)

    module_stats
    |> Enum.map(fn entry ->
      Map.put(entry, :suggested_weight, max(1, round(entry.duration_us / median)))
    end)
    |> Enum.filter(fn entry -> entry.suggested_weight != entry.current_weight end)
    |> Enum.sort_by(& &1.duration_us, :desc)
  end

  defp aggregate_by_module(tests) do
    tests
    |> Enum.group_by(& &1["module"])
    |> Enum.map(fn {module, module_tests} ->
      total = module_tests |> Enum.map(& &1["duration_us"]) |> Enum.sum()
      weight = module_tests |> List.first() |> Map.get("weight", 1)
      {module, total, weight}
    end)
  end

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

  def run(result_dir, _opts, color) do
    outcomes_by_suite = load_outcomes(result_dir)

    if outcomes_by_suite == %{} do
      Mix.shell().info("No outcomes.json files found in #{result_dir}")
    else
      print_suggestions(suggest_weights(outcomes_by_suite), color)
    end
  end

  defp load_outcomes(result_dir) do
    result_dir
    |> Path.join("**/outcomes.json")
    |> Path.wildcard()
    |> Enum.reduce(%{}, fn path, acc ->
      data = path |> File.read!() |> :json.decode()
      Map.put(acc, data["suite"], data["tests"])
    end)
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
