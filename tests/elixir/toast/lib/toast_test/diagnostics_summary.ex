defmodule ToastTest.DiagnosticsSummary do
  @moduledoc "Summarizes suite results: sanitizer detection, artifact inventory, exit codes."

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
      %{suite_result: %ToastTest.SuiteResult{} = sr} = suite ->
        %{
          name: inspect(suite[:suite_module]),
          log_files: extract_log_files(sr.deployments),
          sanitizer_files: extract_sanitizer_files(sr.issues),
          core_dumps: extract_core_dumps(sr.issues)
        }

      suite ->
        %{
          name: inspect(suite[:suite_module]),
          log_files: [],
          sanitizer_files: [],
          core_dumps: []
        }
    end)
  end

  @doc "Compute exit code from aggregated run results."
  @spec exit_code(map()) :: 0 | 1 | 2 | 3 | 4
  def exit_code(results) do
    # Monotonic severity: 4 (crash) > 3 (infra) > 2 (sanitizer) > 1 (test failures) > 0
    cond do
      results.server_crashed -> 4
      results.infrastructure_failure -> 3
      results.sanitizer_errors -> 2
      results.test_failures > 0 -> 1
      true -> 0
    end
  end

  defp extract_log_files(deployments) when is_map(deployments) do
    for {_did, dep} <- deployments,
        {_sid, server} <- dep.servers,
        path = server[:log_file],
        path != nil,
        do: path
  end

  defp extract_log_files(_), do: []

  defp extract_sanitizer_files(issues) when is_list(issues) do
    issues
    |> Enum.filter(&(&1.type == :sanitizer_report))
    |> Enum.flat_map(fn
      %{detail: %{file: path}} when is_binary(path) -> [path]
      _ -> []
    end)
    |> Enum.uniq()
  end

  defp extract_sanitizer_files(_), do: []

  defp extract_core_dumps(issues) when is_list(issues) do
    issues
    |> Enum.filter(&(&1.type == :crash))
    |> Enum.flat_map(&core_dump_paths_from_detail(&1.detail))
  end

  defp extract_core_dumps(_), do: []

  defp core_dump_paths_from_detail(%{coredump_paths: paths}) when is_list(paths), do: paths
  defp core_dump_paths_from_detail(_), do: []
end
