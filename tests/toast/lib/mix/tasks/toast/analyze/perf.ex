################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule Mix.Tasks.Toast.Analyze.Perf do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3, format_duration_us: 1]

  alias Mix.Tasks.Toast.Analyze.Data

  @bar_width 30

  def run(result_dir, opts, color) do
    results =
      result_dir
      |> Data.load_results()
      |> Data.maybe_filter_suite(opts[:suite])

    top = Keyword.get(opts, :top, 20)

    case opts[:module] do
      nil -> print_suite_module_breakdown(results, top, color)
      prefix -> print_module_test_breakdown(results, prefix, top, color)
    end
  end

  defp print_suite_module_breakdown(results, top, color) do
    Mix.shell().info(
      "Legend: " <>
        colorize("██", :yellow, color) <>
        " setup  " <>
        colorize("██", :green, color) <>
        " tests  " <>
        colorize("██", :magenta, color) <>
        " teardown  " <>
        "░░ remaining"
    )

    Enum.each(results, fn result ->
      modules = result.modules
      if map_size(modules) == 0, do: :ok, else: print_suite_modules(result, modules, top, color)
    end)
  end

  defp print_suite_modules(result, modules, top, color) do
    test_count = modules |> Map.values() |> Enum.flat_map(& &1.tests) |> length()
    suite_duration_us = result.times_us[:run] || 0

    Mix.shell().info("")

    Mix.shell().info(
      colorize(
        "#{result.suite} (#{format_duration_us(suite_duration_us)} — #{test_count} tests)",
        :bright,
        color
      )
    )

    module_stats =
      modules
      |> Enum.map(fn {mod, data} -> module_timing(mod, data) end)
      |> Enum.sort_by(& &1.total_us, :desc)
      |> Enum.take(top)

    # Column widths
    mod_width =
      module_stats
      |> Enum.map(&String.length(format_module_name(&1)))
      |> Enum.max(fn -> 6 end)
      |> max(6)

    header =
      " " <>
        String.pad_trailing("Module", mod_width) <>
        "  Duration  Tests  Setup  Teardown"

    Mix.shell().info(colorize(header, :cyan, color))

    Enum.each(module_stats, fn stat ->
      pct = if suite_duration_us > 0, do: stat.total_us / suite_duration_us * 100, else: 0

      bar = phase_bar(stat, suite_duration_us, color)

      row =
        " " <>
          String.pad_trailing(format_module_name(stat), mod_width) <>
          "  " <>
          String.pad_leading(format_duration_us(stat.total_us), 8) <>
          "  " <>
          String.pad_leading(to_string(stat.test_count), 5) <>
          "  " <>
          String.pad_leading(format_duration_us(stat.setup_us), 5) <>
          "  " <>
          String.pad_leading(format_duration_us(stat.teardown_us), 8) <>
          "    " <>
          bar <>
          String.pad_leading("#{Float.round(pct, 0) |> trunc()}%", 5)

      Mix.shell().info(row)
    end)
  end

  defp print_module_test_breakdown(results, prefix, top, color) do
    matches =
      for result <- results,
          {mod, data} <- result.modules,
          display_name = ToastTest.Formatting.display_module_name(mod),
          match_module?(display_name, prefix) do
        {result.suite, mod, data, display_name}
      end

    case matches do
      [] ->
        Mix.raise("No module matching \"#{prefix}\" found.")

      [{suite, mod, data, _}] ->
        print_test_breakdown(suite, mod, data, top, color)

      multiple ->
        names =
          Enum.map_join(multiple, "\n  ", fn {suite, _mod, _data, name} ->
            "#{name} (#{suite})"
          end)

        Mix.raise("Ambiguous module prefix \"#{prefix}\". Matches:\n  #{names}")
    end
  end

  defp match_module?(mod_name, prefix) do
    downcased = String.downcase(mod_name)
    pattern = String.downcase(prefix)
    String.starts_with?(downcased, pattern) or String.contains?(downcased, pattern)
  end

  defp print_test_breakdown(suite, mod, data, top, color) do
    stat = module_timing(mod, data)

    Mix.shell().info("")

    Mix.shell().info(
      colorize(
        "#{stat.display_name} (#{suite}) — #{format_duration_us(stat.total_us)} total, " <>
          "setup #{format_duration_us(stat.setup_us)}, teardown #{format_duration_us(stat.teardown_us)}",
        :bright,
        color
      )
    )

    tests_us = stat.tests_us
    header = " " <> String.pad_trailing("Test", 40) <> "  Duration  Outcome"
    Mix.shell().info(colorize(header, :cyan, color))

    test_stats =
      data.tests
      |> Enum.sort_by(& &1.duration_us, :desc)
      |> Enum.take(top)

    Enum.each(test_stats, fn test ->
      name = ToastTest.Formatting.display_test_name(test.name)
      name = if String.length(name) > 40, do: String.slice(name, 0, 37) <> "...", else: name
      pct = if tests_us > 0, do: test.duration_us / tests_us * 100, else: 0

      bar = single_bar(test.duration_us, tests_us, :green, color)

      outcome_color =
        case test.outcome do
          :passed -> :green
          :failed -> :red
          _ -> :faint
        end

      row =
        " " <>
          String.pad_trailing(name, 40) <>
          "  " <>
          String.pad_leading(format_duration_us(test.duration_us), 8) <>
          "  " <>
          colorize(String.pad_trailing(Atom.to_string(test.outcome), 8), outcome_color, color) <>
          " " <>
          bar <>
          String.pad_leading("#{Float.round(pct, 0) |> trunc()}%", 5)

      Mix.shell().info(row)
    end)
  end

  defp module_timing(mod, data) do
    total_us = datetime_diff_us(data.started_at, data.finished_at)

    setup_us =
      if data.setup_finished_at,
        do: datetime_diff_us(data.started_at, data.setup_finished_at),
        else: 0

    teardown_us =
      if data.teardown_started_at,
        do: datetime_diff_us(data.teardown_started_at, data.finished_at),
        else: 0

    tests_us = max(total_us - setup_us - teardown_us, 0)

    %{
      module: mod,
      display_name: ToastTest.Formatting.display_module_name(mod),
      total_us: total_us,
      setup_us: setup_us,
      tests_us: tests_us,
      teardown_us: teardown_us,
      test_count: length(data.tests)
    }
  end

  defp datetime_diff_us(from, to) when not is_nil(from) and not is_nil(to) do
    DateTime.diff(to, from, :microsecond) |> max(0)
  end

  defp datetime_diff_us(_, _), do: 0

  defp phase_bar(stat, suite_total_us, color) when suite_total_us > 0 do
    total_cells = @bar_width
    fraction = stat.total_us / suite_total_us
    filled = round(fraction * total_cells) |> max(if(stat.total_us > 0, do: 1, else: 0))

    # Distribute filled cells among phases proportionally (largest-remainder method)
    phase_total = stat.setup_us + stat.tests_us + stat.teardown_us

    {setup_cells, test_cells, td_cells} =
      if phase_total > 0 do
        distribute_cells(
          [{stat.setup_us, :setup}, {stat.tests_us, :tests}, {stat.teardown_us, :td}],
          phase_total,
          filled
        )
      else
        {0, filled, 0}
      end

    empty = total_cells - setup_cells - test_cells - td_cells

    colorize(String.duplicate("█", setup_cells), :yellow, color) <>
      colorize(String.duplicate("█", test_cells), :green, color) <>
      colorize(String.duplicate("█", td_cells), :magenta, color) <>
      String.duplicate("░", empty)
  end

  defp phase_bar(_stat, _suite_total_us, _color) do
    String.duplicate("░", @bar_width)
  end

  # Largest-remainder method: floor each share, then distribute leftover cells
  # to phases with the largest fractional remainders.
  defp distribute_cells(phases, total_us, budget) do
    shares =
      Enum.map(phases, fn {us, label} ->
        exact = us / total_us * budget
        {label, trunc(exact), exact - trunc(exact)}
      end)

    floored = Enum.reduce(shares, 0, fn {_, f, _}, acc -> acc + f end)
    leftover = budget - floored

    # Award leftover cells to phases with largest fractional part
    awarded =
      shares
      |> Enum.sort_by(fn {_, _, frac} -> frac end, :desc)
      |> Enum.with_index()
      |> Enum.map(fn {{label, f, _frac}, i} ->
        {label, f + if(i < leftover, do: 1, else: 0)}
      end)

    result = Map.new(awarded)
    {Map.get(result, :setup, 0), Map.get(result, :tests, 0), Map.get(result, :td, 0)}
  end

  defp single_bar(value_us, total_us, bar_color, color) when total_us > 0 do
    filled = round(value_us / total_us * @bar_width) |> max(if(value_us > 0, do: 1, else: 0))
    empty = @bar_width - filled

    colorize(String.duplicate("█", filled), bar_color, color) <>
      String.duplicate("░", max(empty, 0))
  end

  defp single_bar(_, _, _, _), do: String.duplicate("░", @bar_width)

  defp format_module_name(%{display_name: name}), do: name
end
