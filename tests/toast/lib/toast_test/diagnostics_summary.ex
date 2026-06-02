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

defmodule ToastTest.DiagnosticsSummary do
  @moduledoc "Summarizes suite results: sanitizer detection, artifact inventory, exit codes."

  @doc "Check whether any suite has server crash issues."
  @spec has_server_crash?([map()]) :: boolean()
  def has_server_crash?(suites), do: has_issue_type?(suites, :crash)

  @doc "Check whether any suite has timeout issues (e.g., startup/shutdown timeout)."
  @spec has_timeout?([map()]) :: boolean()
  def has_timeout?(suites), do: has_issue_type?(suites, :timeout)

  @doc "Check whether any suite has sanitizer error issues."
  @spec has_sanitizer_errors?([map()]) :: boolean()
  def has_sanitizer_errors?(suites), do: has_issue_type?(suites, :sanitizer_report)

  @doc "Check whether any suite has infrastructure issues (e.g., port exhaustion)."
  @spec has_infrastructure?([map()]) :: boolean()
  def has_infrastructure?(suites), do: has_issue_type?(suites, :infrastructure)

  defp has_issue_type?(suites, type) do
    Enum.any?(suites, fn
      %{suite_result: %ToastTest.SuiteResult{issues: issues}} ->
        Enum.any?(issues, &(&1.type == type))

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
