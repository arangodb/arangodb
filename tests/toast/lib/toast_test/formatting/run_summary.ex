defmodule ToastTest.Formatting.RunSummary do
  @moduledoc false

  import ToastTest.Formatting
  alias ToastTest.Formatting.{Color, Utils}

  @spec print([ToastTest.SuiteResult.t()], non_neg_integer()) :: :ok
  def print(suite_results, elapsed_us) do
    {mod_counts, test_counts} = count(suite_results)
    colors = IO.ANSI.enabled?()

    Utils.print_header("SUMMARY", colors, Color.summary())
    IO.puts(format_line("Modules", mod_counts, colors))
    IO.puts(format_line("Test cases", test_counts, colors))
    IO.puts("  Runtime:     #{format_duration_us(elapsed_us)}")

    :ok
  end

  defp count(suite_results) do
    Enum.reduce(suite_results, {zero_module_counts(), zero_counts()}, fn sr,
                                                                         {mod_acc, test_acc} ->
      {mod_counts, test_counts} = count_suite(sr)
      {merge(mod_acc, mod_counts), merge(test_acc, test_counts)}
    end)
  end

  defp count_suite(%ToastTest.SuiteResult{modules: modules}) do
    Enum.reduce(modules, {zero_module_counts(), zero_counts()}, fn {_mod, mod_result},
                                                                   {m_acc, t_acc} ->
      test_counts = count_tests(mod_result.tests)
      mod_counts = module_counts(test_counts)
      {merge(m_acc, mod_counts), merge(t_acc, test_counts)}
    end)
  end

  defp count_tests(tests) do
    Enum.reduce(tests, zero_counts(), fn test, acc ->
      case test.outcome do
        :passed ->
          %{acc | total: acc.total + 1, passed: acc.passed + 1}

        :failed ->
          %{acc | total: acc.total + 1, failed: acc.failed + 1}

        _ ->
          %{acc | total: acc.total + 1, skipped: acc.skipped + 1}
      end
    end)
  end

  defp module_counts(test_counts) do
    outcomes = Enum.count([:passed, :failed, :skipped], &(test_counts[&1] > 0))

    cond do
      outcomes > 1 ->
        %{total: 1, passed: 0, mixed: 1, failed: 0, skipped: 0}

      test_counts.failed > 0 ->
        %{total: 1, passed: 0, mixed: 0, failed: 1, skipped: 0}

      test_counts.passed > 0 ->
        %{total: 1, passed: 1, mixed: 0, failed: 0, skipped: 0}

      true ->
        %{total: 1, passed: 0, mixed: 0, failed: 0, skipped: 1}
    end
  end

  defp zero_counts, do: %{total: 0, passed: 0, failed: 0, skipped: 0}

  defp zero_module_counts,
    do: %{total: 0, passed: 0, mixed: 0, failed: 0, skipped: 0}

  defp merge(a, b), do: Map.merge(a, b, fn _k, v1, v2 -> v1 + v2 end)

  # Always displayed regardless of value:
  @always_show MapSet.new([:total, :passed, :failed])

  @outcomes [
    total: :default_color,
    passed: :green,
    mixed: :yellow,
    failed: :red,
    skipped: :yellow
  ]

  defp format_line(label, counts, colors) do
    padded = String.pad_trailing(label <> ":", 13)

    items =
      for {key, color} <- @outcomes,
          Map.has_key?(counts, key),
          counts[key] > 0 or MapSet.member?(@always_show, key) do
        colorize_count(counts[key], " #{key}", color, colors)
      end
      |> Enum.intersperse(", ")

    IO.iodata_to_binary(["  ", padded | items])
  end

  defp colorize_count(0, suffix, _color, _colors), do: "0#{suffix}"
  defp colorize_count(n, suffix, :default_color, _colors), do: "#{n}#{suffix}"
  defp colorize_count(n, suffix, color, colors), do: colorize("#{n}#{suffix}", color, colors)
end
