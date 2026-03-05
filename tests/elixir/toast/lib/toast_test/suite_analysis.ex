defmodule ToastTest.SuiteAnalysis do
  @moduledoc false

  def finalize(suite_module, test_results, diagnostics, result_dir) do
    tests = if test_results, do: ToastTest.ResultFormatter.flat_tests(test_results)

    sanitizer_matching = Toast.Diagnostics.SanitizerMatcher.match(diagnostics, tests)
    crash_matching = Toast.Diagnostics.CrashMatcher.match(diagnostics, tests)
    log_matching = Toast.Diagnostics.LogMatcher.match(diagnostics, tests)

    print_diagnostics_report(
      diagnostics,
      test_results,
      crash_matching,
      sanitizer_matching,
      log_matching
    )

    suite_name = derive_suite_name(suite_module)

    analysis = %ToastTest.ResultExporter.AnalysisData{
      diagnostics: diagnostics,
      sanitizer_matching: sanitizer_matching,
      crash_matching: crash_matching,
      log_matching: log_matching
    }

    ToastTest.ResultExporter.export(suite_name, test_results, analysis, result_dir)
  end

  defp derive_suite_name(suite_module) do
    suite_module |> Module.split() |> hd() |> Macro.underscore()
  end

  defp print_diagnostics_report(
         diagnostics,
         test_results,
         crash_matching,
         sanitizer_matching,
         log_matching
       ) do
    alias Toast.Diagnostics.Summary

    if crash_matching.matched == [] and crash_matching.unmatched == [] do
      maybe_print(Summary.format_crashed_servers(diagnostics))
    end

    crash_affected = find_crash_affected_tests(crash_matching, test_results)
    maybe_print(Summary.format_crash_attribution(crash_matching, crash_affected))
    maybe_print(Summary.format_sanitizer_issues(sanitizer_matching))
    maybe_print(Summary.format_log_issues(log_matching))
  end

  defp maybe_print(nil), do: :ok
  defp maybe_print(text), do: IO.puts(text)

  def find_crash_affected_tests(_crash_matching, nil), do: []

  def find_crash_affected_tests(%{matched: matched, unmatched: unmatched}, test_results) do
    all_crashes = Enum.map(matched, & &1.crash) ++ unmatched
    timestamps = all_crashes |> Enum.map(& &1.timestamp) |> Enum.reject(&is_nil/1)

    case timestamps do
      [] ->
        []

      _ ->
        earliest = Enum.min(timestamps, DateTime)
        attributed = MapSet.new(matched, fn m -> {m.module, m.test} end)

        test_results
        |> ToastTest.ResultFormatter.flat_tests()
        |> Enum.filter(fn t ->
          t.outcome == :failed and
            t.started_at != nil and
            DateTime.compare(t.started_at, earliest) in [:gt, :eq] and
            not MapSet.member?(attributed, {t.module, t.name})
        end)
    end
  end
end
