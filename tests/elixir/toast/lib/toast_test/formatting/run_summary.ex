defmodule ToastTest.Formatting.RunSummary do
  @moduledoc false

  import ToastTest.Formatting

  @spec print([ToastTest.SuiteResult.t()], non_neg_integer()) :: :ok
  def print(suite_results, elapsed_us) do
    {mod_counts, test_counts} = count(suite_results)
    colors = IO.ANSI.enabled?()
    bar = String.duplicate("\u2500", 80)

    IO.puts("")
    IO.puts(colorize(bar, :blue, colors))
    IO.puts(colorize(" SUMMARY", :blue, colors))

    IO.puts(format_line("Modules", mod_counts, colors))
    IO.puts(format_line("Test cases", test_counts, colors))
    IO.puts("  Runtime:     #{format_duration(elapsed_us)}")

    :ok
  end

  defp count(suite_results) do
    Enum.reduce(suite_results, {zero_counts(), zero_counts()}, fn sr, {mod_acc, test_acc} ->
      {mod_counts, test_counts} = count_suite(sr)
      {merge(mod_acc, mod_counts), merge(test_acc, test_counts)}
    end)
  end

  defp count_suite(%ToastTest.SuiteResult{modules: modules}) do
    Enum.reduce(modules, {zero_counts(), zero_counts()}, fn {_mod, mod_result}, {m_acc, t_acc} ->
      test_counts = count_tests(mod_result.tests)
      mod_counts = module_counts(test_counts)
      {merge(m_acc, mod_counts), merge(t_acc, test_counts)}
    end)
  end

  defp count_tests(tests) do
    Enum.reduce(tests, zero_counts(), fn test, acc ->
      case test.outcome do
        :passed ->
          %{acc | total: acc.total + 1, successful: acc.successful + 1}

        :failed ->
          %{acc | total: acc.total + 1, successful: acc.successful + 1, failed: acc.failed + 1}

        _ ->
          %{acc | total: acc.total + 1, skipped: acc.skipped + 1}
      end
    end)
  end

  defp module_counts(test_counts) do
    cond do
      test_counts.total == 0 ->
        %{total: 1, successful: 0, failed: 0, skipped: 1}

      test_counts.failed > 0 ->
        %{total: 1, successful: 1, failed: 1, skipped: 0}

      test_counts.successful > 0 ->
        %{total: 1, successful: 1, failed: 0, skipped: 0}

      true ->
        %{total: 1, successful: 0, failed: 0, skipped: 1}
    end
  end

  defp zero_counts, do: %{total: 0, successful: 0, failed: 0, skipped: 0}

  defp merge(a, b) do
    %{
      total: a.total + b.total,
      successful: a.successful + b.successful,
      failed: a.failed + b.failed,
      skipped: a.skipped + b.skipped
    }
  end

  defp format_line(label, counts, colors) do
    padded = String.pad_trailing(label <> ":", 13)

    [
      "  #{padded}",
      colorize_count(counts.total, " total", :default_color, colors),
      ", ",
      colorize_count(counts.successful, " successful", :green, colors),
      ", ",
      colorize_count(counts.failed, " failed", :red, colors),
      ", ",
      colorize_count(counts.skipped, " skipped", :yellow, colors)
    ]
    |> IO.iodata_to_binary()
  end

  defp format_duration(us) when us < 1_000, do: "#{us}µs"
  defp format_duration(us) when us < 1_000_000, do: "#{Float.round(us / 1_000, 1)}ms"

  defp format_duration(us) do
    total_seconds = div(us, 1_000_000)
    minutes = div(total_seconds, 60)
    seconds = rem(total_seconds, 60)
    frac = us |> rem(1_000_000) |> div(100_000)

    if minutes > 0 do
      "#{minutes}m #{seconds}.#{frac}s"
    else
      "#{seconds}.#{frac}s"
    end
  end

  defp colorize_count(0, suffix, _color, _colors), do: "0#{suffix}"
  defp colorize_count(n, suffix, :default_color, _colors), do: "#{n}#{suffix}"
  defp colorize_count(n, suffix, color, colors), do: colorize("#{n}#{suffix}", color, colors)
end
