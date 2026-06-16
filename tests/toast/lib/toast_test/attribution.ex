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

defmodule ToastTest.Attribution do
  @moduledoc """
  Pure issue production from already-enriched inputs.

  Given test failures, enriched crash events (`effective_at`, crash log lines,
  coredump reports — see `ToastTest.Enrichment`), parsed sanitizer reports,
  timeout kills, and infrastructure events, produces a flat list of
  `SuiteResult.issue()` maps. No file I/O happens here: enrichment turns paths
  into data, this module decides (window-matching, scoping, confidence, issue
  assembly) over that data.
  """

  alias ToastTest.Attribution.TimeWindows

  import Toast.Utils, only: [maybe_put: 3]

  require Logger

  @spec run(
          ToastTest.ResultCollector.test_data(),
          [ToastTest.CrashEvent.t()],
          [map()],
          TimeWindows.windows(),
          keyword()
        ) ::
          {[ToastTest.SuiteResult.issue()], [ToastTest.SuiteResult.coredump_report()]}
  def run(test_data, enriched_crashes, sanitizer_reports, windows, opts \\ [])
      when is_list(enriched_crashes) and is_list(sanitizer_reports) and is_list(opts) do
    timeout_kills = Keyword.get(opts, :timeout_kills, [])
    infrastructure_events = Keyword.get(opts, :infrastructure_issues, [])

    issues =
      infrastructure_issues(infrastructure_events, windows) ++
        test_failure_issues(test_data.failures) ++
        crash_issues(enriched_crashes, windows) ++
        sanitizer_issues(sanitizer_reports, windows) ++
        timeout_issues(timeout_kills)

    coredump_reports = Enum.flat_map(enriched_crashes, & &1.coredump_reports)

    breakdown =
      issues
      |> Enum.frequencies_by(& &1.type)
      |> Enum.map_join(", ", fn {t, n} -> "#{n} #{t}" end)

    Logger.debug(
      "Attribution: #{length(issues)} issue(s)#{if breakdown != "", do: " (#{breakdown})", else: ""}"
    )

    {issues, coredump_reports}
  end

  # --- Test failures ---

  defp test_failure_issues(failures) do
    Enum.map(failures, fn test ->
      %{
        type: :test_failure,
        scope: {:test, test.module, test.name},
        confidence: nil,
        detail: %{test: test}
      }
    end)
  end

  # --- Crashes ---

  # Pure over enriched crash events: `effective_at` (the log-resolved crash time,
  # for windowing/display), `coredump_reports`, `log_file`, and `crash_lines` are
  # all produced by `ToastTest.Enrichment`. The raw `crash_info` (incl. its
  # untouched detection `timestamp`) is carried through as process-fact provenance.
  defp crash_issues(enriched_crashes, windows) do
    Enum.map(enriched_crashes, fn event ->
      {scope, confidence, phase} = TimeWindows.attribute(event.effective_at, windows)
      coredump_paths = Enum.map(event.coredump_reports, & &1.core_path)

      detail =
        %{server: event.server_id, crash_info: event.crash_info, effective_at: event.effective_at}
        |> maybe_put(:phase, phase)
        |> maybe_put_coredump_paths(coredump_paths)
        |> maybe_put(:log_file, event.log_file)
        |> maybe_put(:crash_lines, event.crash_lines)

      %{type: :crash, scope: scope, confidence: confidence, detail: detail}
    end)
  end

  defp maybe_put_coredump_paths(detail, []), do: detail
  defp maybe_put_coredump_paths(detail, paths), do: Map.put(detail, :coredump_paths, paths)

  # --- Infrastructure issues ---

  # Infrastructure events observed live during the run (e.g. port exhaustion),
  # attributed by their own timestamp.
  defp infrastructure_issues(infrastructure_events, windows) do
    Enum.map(infrastructure_events, fn event ->
      {scope, confidence, phase} = TimeWindows.attribute(event.timestamp, windows)

      detail =
        event.detail
        |> Map.merge(%{subtype: event.subtype, timestamp: event.timestamp})
        |> maybe_put(:phase, phase)

      %{type: :infrastructure, scope: scope, confidence: confidence, detail: detail}
    end)
  end

  # --- Timeouts ---

  # Pure over timeout kills already enriched with per-server coredump paths by
  # `ToastTest.Enrichment.enrich_timeout_kills/2`.
  defp timeout_issues(timeout_kills) do
    Enum.map(timeout_kills, fn kill ->
      %{
        type: :infrastructure,
        scope: :suite,
        confidence: :high,
        detail: %{
          subtype: :timeout,
          source: kill.source,
          reason: kill.reason,
          timestamp: kill.timestamp,
          servers: kill.servers
        }
      }
    end)
  end

  # --- Sanitizer reports ---

  # Pure over reports already read + parsed by `ToastTest.Enrichment`.
  defp sanitizer_issues(sanitizer_reports, windows) do
    Enum.map(sanitizer_reports, fn report ->
      {scope, confidence, phase} =
        if is_integer(report.timestamp) do
          TimeWindows.attribute(report.timestamp, windows)
        else
          {:suite, nil, nil}
        end

      detail =
        %{
          server: report.server_id,
          file: report.file,
          report: report.content,
          timestamp: report.timestamp,
          kind: report.kind
        }
        |> maybe_put(:phase, phase)

      %{type: :sanitizer_report, scope: scope, confidence: confidence, detail: detail}
    end)
  end
end
