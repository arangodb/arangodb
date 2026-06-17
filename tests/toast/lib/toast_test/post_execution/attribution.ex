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

defmodule ToastTest.PostExecution.Attribution.Inputs do
  @moduledoc """
  The explicit input bundle `Attribution.run/2` decides over — every fact-list
  it needs, named and required rather than smuggled through `opts`. Built by
  PostExecution from the enriched Context. The temporal frame (`windows`) is
  passed separately: it is the frame issues are attributed *within*, not a fact.
  """

  @type t :: %__MODULE__{
          failures: [ExUnit.Test.t()],
          crashes: [ToastTest.CrashEvent.t()],
          sanitizer_reports: [map()],
          timeout_kills: [map()],
          infrastructure: [map()]
        }

  defstruct failures: [],
            crashes: [],
            sanitizer_reports: [],
            timeout_kills: [],
            infrastructure: []
end

defmodule ToastTest.PostExecution.Attribution do
  @moduledoc """
  Pure issue production from an already-enriched `Attribution.Inputs` bundle.

  Given test failures, enriched crash events (`effective_at`, crash log lines,
  coredump reports — see `ToastTest.PostExecution.Enrichment`), parsed sanitizer reports,
  timeout kills, and infrastructure events, produces a flat list of
  `SuiteResult.issue()` maps. No file I/O happens here: enrichment turns paths
  into data, this module decides (window-matching, scoping, confidence, issue
  assembly) over that data.

  The coredump-report aggregate is *not* produced here — it is a plain
  projection of the enriched crashes (`flat_map(crashes, & &1.coredump_reports)`)
  that PostExecution extracts itself; it is not an attribution decision.
  """

  alias ToastTest.PostExecution.Attribution.Inputs
  alias ToastTest.TimeWindows

  import Toast.Utils, only: [maybe_put: 3]

  require Logger

  @spec run(Inputs.t(), TimeWindows.windows()) :: [ToastTest.SuiteResult.issue()]
  def run(%Inputs{} = inputs, windows) do
    issues =
      infrastructure_issues(inputs.infrastructure, windows) ++
        test_failure_issues(inputs.failures) ++
        crash_issues(inputs.crashes, windows) ++
        sanitizer_issues(inputs.sanitizer_reports, windows) ++
        timeout_issues(inputs.timeout_kills)

    breakdown =
      issues
      |> Enum.frequencies_by(& &1.type)
      |> Enum.map_join(", ", fn {t, n} -> "#{n} #{t}" end)

    Logger.debug(
      "Attribution: #{length(issues)} issue(s)#{if breakdown != "", do: " (#{breakdown})", else: ""}"
    )

    issues
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
  # all produced by `ToastTest.PostExecution.Enrichment`. The raw `crash_info` (incl. its
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
  # `ToastTest.PostExecution.Enrichment.enrich_timeout_kills/2`.
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

  # Pure over reports already read + parsed by `ToastTest.PostExecution.Enrichment`.
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
