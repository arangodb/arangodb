defmodule ToastTest.DiagnosticsSummary do
  @moduledoc "Query helpers for suite diagnostic results."

  @doc """
  Check whether any suite has sanitizer errors.

  Examines `SuiteResult.issues` for `:sanitizer_report` type issues.
  """
  @spec has_sanitizer_errors?([map()]) :: boolean()
  def has_sanitizer_errors?(suites) do
    Enum.any?(suites, fn
      %{suite_result: %ToastTest.SuiteResult{issues: issues}} ->
        Enum.any?(issues, &(&1.type == :sanitizer_report))

      _ ->
        false
    end)
  end

  @doc """
  Build a diagnostics summary for each suite for CI artifact packaging.

  Extracts file paths (core dumps, etc.) from `SuiteResult.issues`.
  """
  @spec build_suite_diagnostics([map()]) :: [map()]
  def build_suite_diagnostics(suites) do
    Enum.map(suites, fn
      %{suite_result: %ToastTest.SuiteResult{issues: issues}} = suite ->
        %{
          name: inspect(suite[:suite_module]),
          core_dumps: extract_core_dumps(issues)
        }

      suite ->
        %{
          name: inspect(suite[:suite_module]),
          core_dumps: []
        }
    end)
  end

  defp extract_core_dumps(issues) when is_list(issues) do
    issues
    |> Enum.filter(&(&1.type == :crash))
    |> Enum.flat_map(&core_dump_paths_from_detail(&1.detail))
  end

  defp extract_core_dumps(_), do: []

  defp core_dump_paths_from_detail(%{coredump_paths: paths}) when is_list(paths), do: paths
  defp core_dump_paths_from_detail(_), do: []
end
