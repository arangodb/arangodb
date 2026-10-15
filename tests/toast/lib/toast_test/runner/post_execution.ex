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

defmodule ToastTest.Runner.PostExecution do
  @moduledoc false

  alias ToastTest.{EventStore, SuiteResult}
  alias ToastTest.Formatting.{Color, Utils}
  alias ToastTest.Runner.ResultBuilder

  require Logger

  @doc """
  Builds a `SuiteResult` after test execution completes.

  Analyses the EventStore data (crashes, timeout kills, infrastructure issues),
  collects artifacts from `servers` (coredumps, sanitizer files), gathers server
  logs, and writes results to disk.

  The caller is responsible for stopping the deployment and passing the resulting
  server map. An empty map is valid (manual mode, excluded suites, failed
  deployments) — the analysis still runs using EventStore data.
  """
  @spec run(map(), map(), ToastTest.Config.t(), Path.t() | nil) :: SuiteResult.t()
  def run(servers, test_data, %ToastTest.Config{} = test_config, pcap_path \\ nil) do
    try do
      build_suite_result(servers, test_data, test_config, pcap_path)
    rescue
      e ->
        Logger.warning(
          "build_suite_result crashed, returning degraded result: " <>
            "#{Exception.format(:error, e, __STACKTRACE__)}"
        )

        SuiteResult.build(test_data, [],
          warnings: ["Post-execution analysis failed: #{Exception.message(e)}"]
        )
    end
  end

  defp build_suite_result(servers, test_data, test_config, pcap_path) do
    Utils.print_header("SUITE FINISHED", IO.ANSI.enabled?(), Color.info())

    Logger.info("Running post-execution analysis...")

    snapshot = EventStore.snapshot()

    Logger.debug("Collecting artifacts")
    artifact_opts = [coredump_dir: test_config.coredump_dir, not_before: test_data.started_at]

    artifacts =
      ToastTest.ArtifactCollector.collect(servers, snapshot.pids_by_server, artifact_opts)

    Logger.debug("Running attribution")

    crash_events =
      snapshot.unexpected_crashes
      |> Enum.map(&ResultBuilder.to_crash_event/1)
      |> ToastTest.Attribution.resolve_crash_timestamps(artifacts)

    test_data = ToastTest.Attribution.Invalidation.apply(test_data, crash_events)

    {issues, coredump_reports} =
      ToastTest.Attribution.run(test_data, artifacts, crash_events,
        timeout_kills: snapshot.timeout_kills,
        analyzer_opts: build_coredump_analyzer_opts(test_config)
      )

    Logger.debug("Collecting server logs")
    windows = ToastTest.Attribution.TimeWindows.build(test_data)
    all_log_files = ResultBuilder.collect_log_files(snapshot.servers)
    server_logs = ToastTest.Attribution.ServerLogs.collect(issues, all_log_files, windows)

    infra_issues = build_infrastructure_issues(snapshot.infrastructure_issues, windows)
    issues = infra_issues ++ issues

    Logger.debug("Building results (#{length(issues)} issues found)")
    active_sanitizers = test_config.active_sanitizers

    warnings =
      ResultBuilder.coredump_warnings(
        crash_events,
        artifacts,
        test_config.coredump_dir,
        active_sanitizers
      )

    deployments = ResultBuilder.build_deployments(snapshot, server_logs)

    traffic = extract_traffic(pcap_path)

    suite_result =
      SuiteResult.build(test_data, issues,
        warnings: warnings,
        deployments: deployments,
        coredumps: coredump_reports,
        events: snapshot.events,
        traffic: traffic,
        pcap_path: pcap_path
      )

    SuiteResult.write_all(suite_result, test_config.result_dir)
    suite_result
  end

  defp build_infrastructure_issues([], _windows), do: []

  defp build_infrastructure_issues(infra_events, windows) do
    Enum.map(infra_events, fn event ->
      {scope, confidence, phase} =
        ToastTest.Attribution.TimeWindows.attribute(event.timestamp, windows)

      detail =
        event.detail
        |> Map.merge(%{subtype: event.subtype, timestamp: event.timestamp})
        |> Toast.Utils.maybe_put(:phase, phase)

      %{type: :infrastructure, scope: scope, confidence: confidence, detail: detail}
    end)
  end

  defp build_coredump_analyzer_opts(test_config) do
    opts = [timeout: test_config.coredump_timeout]

    case Toast.Diagnostics.Coredump.resolve_debugger(test_config.debugger) do
      nil -> opts
      debugger -> [{:debugger, debugger} | opts]
    end
  end

  defp extract_traffic(nil), do: []

  defp extract_traffic(pcap_path) do
    case ToastTest.Traffic.Extraction.extract(pcap_path) do
      {:ok, entries} ->
        entries

      {:error, :tshark_not_found} ->
        Logger.warning(
          "tshark not found — skipping traffic extraction. " <>
            "Install Wireshark/tshark to enable HTTP traffic analysis. " <>
            "Raw pcap file: #{pcap_path}"
        )

        []

      {:error, reason} ->
        Logger.warning("Traffic extraction failed: #{inspect(reason)}")
        []
    end
  end
end
