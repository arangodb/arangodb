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

defmodule ToastTest.PostExecution.Context do
  @moduledoc false

  @type t :: %__MODULE__{
          config: ToastTest.Config.t(),
          pcap_path: Path.t() | nil,
          snapshot: map(),
          windows: ToastTest.TimeWindows.windows(),
          test_data: map(),
          enriched: %{
            artifacts: ToastTest.PostExecution.ArtifactCollector.t(),
            crash_events: [ToastTest.CrashEvent.t()],
            sanitizer_reports: [map()],
            timeout_kills: [map()]
          },
          server_logs: %{optional(String.t()) => list()},
          issues: [ToastTest.SuiteResult.issue()],
          coredumps: [ToastTest.SuiteResult.coredump_report()],
          deployments: %{optional(String.t()) => map()},
          traffic: [map()],
          agency_logs: %{
            optional(String.t()) => [{Toast.timestamp(), Toast.timestamp(), [map()]}]
          },
          warnings: [String.t()]
        }

  # Transient accumulator for one post-execution build — NOT a long-lived domain
  # object. `enriched` is the one cohesive nested bundle (the filesystem-derived
  # inputs attribution decides over). `server_logs` is an intermediate consumed by
  # deployment assembly; it never reaches the SuiteResult. Every field is
  # default-initialized so a degraded (failed) stage simply keeps its default.
  defstruct [
    :config,
    :pcap_path,
    :snapshot,
    :windows,
    :test_data,
    enriched: %{artifacts: %{}, crash_events: [], sanitizer_reports: [], timeout_kills: []},
    server_logs: %{},
    issues: [],
    coredumps: [],
    deployments: %{},
    traffic: [],
    agency_logs: %{},
    warnings: []
  ]
end

defmodule ToastTest.PostExecution do
  @moduledoc false

  alias ToastTest.{EventStore, SuiteResult}
  alias ToastTest.Formatting.{Color, Utils}
  alias ToastTest.PostExecution.Attribution
  alias ToastTest.PostExecution.Context
  alias ToastTest.PostExecution.Enrichment
  alias ToastTest.PostExecution.ResultBuilder

  require Logger

  @doc """
  Builds a `SuiteResult` after test execution completes.

  Analyses the EventStore data (crashes, timeout kills, infrastructure issues),
  collects artifacts (coredumps, sanitizer files), gathers server logs, and
  writes results to disk. All server information is read from the EventStore.
  """
  @spec run(map(), ToastTest.Config.t(), Path.t() | nil) :: SuiteResult.t()
  def run(test_data, %ToastTest.Config{} = test_config, pcap_path \\ nil) do
    test_data
    |> init_context(test_config, pcap_path)
    |> enrich()
    |> attribute()
    |> collect_outputs()
    |> build_and_write()
  rescue
    # Last-resort backstop: every fallible step inside the phases degrades
    # individually (see stage/4), so reaching here means the final assembly
    # itself failed.
    e ->
      Logger.warning(
        "Post-execution assembly crashed, returning degraded result: " <>
          "#{Exception.format(:error, e, __STACKTRACE__)}"
      )

      SuiteResult.build(test_data, [],
        warnings: ["Post-execution analysis failed: #{Exception.message(e)}"]
      )
  end

  # Seed the pipeline: the run's fixed context plus the EventStore-derived timeline.
  defp init_context(test_data, test_config, pcap_path) do
    Utils.print_header("SUITE FINISHED", IO.ANSI.enabled?(), Color.info())
    Logger.info("Running post-execution analysis...")

    snapshot = EventStore.snapshot()

    %Context{
      config: test_config,
      pcap_path: pcap_path,
      test_data: test_data,
      snapshot: snapshot,
      windows: ToastTest.TimeWindows.build(snapshot.events)
    }
  end

  # Filesystem-enrichment phase: discover artifacts, then read/parse them once
  # into the data attribution decides over.
  defp enrich(ctx) do
    Logger.debug("Enriching diagnostics")

    ctx
    |> stage("Artifact collection", &collect_artifacts/1)
    |> stage("Crash enrichment", &enrich_crashes/1, &fall_back_to_raw_crashes/1)
    |> stage("Sanitizer enrichment", &read_sanitizer_reports/1)
    |> put_timeout_kills()
    |> add_coredump_warnings()
  end

  defp collect_artifacts(ctx) do
    servers = build_servers(ctx.snapshot)
    opts = [coredump_dir: ctx.config.coredump_dir, not_before: ctx.windows.suite.started_at]
    put_enriched(ctx, :artifacts, __MODULE__.ArtifactCollector.collect(servers, opts))
  end

  defp enrich_crashes(ctx) do
    {events, warnings} =
      Enrichment.enrich_crashes(
        raw_crashes(ctx),
        ctx.enriched.artifacts,
        build_coredump_analyzer_opts(ctx.config)
      )

    ctx |> put_enriched(:crash_events, events) |> append_warnings(warnings)
  end

  # Custom degraded value: crashes are event-derived and must still produce
  # issues even if enrichment fails wholesale — fall back to the raw detection
  # timestamp.
  defp fall_back_to_raw_crashes(ctx),
    do: put_enriched(ctx, :crash_events, fallback_enriched(raw_crashes(ctx)))

  defp read_sanitizer_reports(ctx) do
    {reports, warnings} = Enrichment.sanitizer_reports(ctx.enriched.artifacts)
    ctx |> put_enriched(:sanitizer_reports, reports) |> append_warnings(warnings)
  end

  defp put_timeout_kills(ctx) do
    put_enriched(
      ctx,
      :timeout_kills,
      Enrichment.enrich_timeout_kills(ctx.snapshot.timeout_kills, ctx.enriched.artifacts)
    )
  end

  defp add_coredump_warnings(ctx) do
    append_warnings(
      ctx,
      ResultBuilder.coredump_warnings(
        ctx.enriched.crash_events,
        ctx.enriched.artifacts,
        ctx.config.coredump_dir,
        ctx.config.active_sanitizers
      )
    )
  end

  # Decision phase (pure): invalidate post-crash test failures, then attribute
  # every issue over the enriched inputs.
  defp attribute(ctx) do
    Logger.debug("Running attribution")

    ctx
    |> stage("Crash invalidation", &invalidate_failures/1)
    |> stage("Attribution", &attribute_issues/1)
  end

  defp invalidate_failures(ctx) do
    test_data =
      Attribution.Invalidation.apply(ctx.test_data, ctx.enriched.crash_events, ctx.windows)

    %{ctx | test_data: test_data}
  end

  defp attribute_issues(ctx) do
    inputs = %Attribution.Inputs{
      failures: ctx.test_data.failures,
      crashes: ctx.enriched.crash_events,
      sanitizer_reports: ctx.enriched.sanitizer_reports,
      timeout_kills: ctx.enriched.timeout_kills,
      infrastructure: ctx.snapshot.infrastructure_issues
    }

    issues = Attribution.run(inputs, ctx.windows)
    coredumps = Enum.flat_map(ctx.enriched.crash_events, & &1.coredump_reports)
    %{ctx | issues: issues, coredumps: coredumps}
  end

  # Output phase: the displays that depend on the attributed issues — per-server
  # log excerpts, deployment summaries, captured traffic.
  defp collect_outputs(ctx) do
    Logger.debug("Collecting outputs")

    ctx
    |> stage("Server log collection", &collect_server_logs/1)
    |> stage("Agency log collection", &collect_agency_logs/1)
    |> stage("Deployment assembly", &assemble_deployments/1)
    |> stage("Traffic extraction", &extract_traffic_into/1)
  end

  defp collect_server_logs(ctx) do
    all_log_files = ResultBuilder.collect_log_files(ctx.snapshot.servers)
    logs = Attribution.ServerLogs.collect(ctx.issues, all_log_files, ctx.windows)
    %{ctx | server_logs: logs}
  end

  defp collect_agency_logs(ctx) do
    logs = Attribution.AgencyLogs.collect(ctx.issues, ctx.snapshot.agency_dumps, ctx.windows)
    %{ctx | agency_logs: logs}
  end

  defp assemble_deployments(ctx),
    do: %{ctx | deployments: ResultBuilder.build_deployments(ctx.snapshot, ctx.server_logs)}

  defp extract_traffic_into(ctx), do: %{ctx | traffic: extract_traffic(ctx.pcap_path)}

  # Terminal sink: assemble the SuiteResult from the accumulated context and
  # write it. Returns the SuiteResult (not a Context).
  defp build_and_write(ctx) do
    suite_result =
      SuiteResult.build(ctx.test_data, ctx.issues,
        windows: ctx.windows,
        warnings: ctx.warnings,
        deployments: ctx.deployments,
        coredumps: ctx.coredumps,
        events: ctx.snapshot.events,
        traffic: ctx.traffic,
        agency_logs: ctx.agency_logs,
        pcap_path: ctx.pcap_path
      )

    stage(ctx, "Writing results", fn c ->
      SuiteResult.write_all(suite_result, c.config.result_dir)
      c
    end)

    suite_result
  end

  # --- pipeline helpers ---

  # Run a fallible step as `ctx -> ctx`. On success returns `fun.(ctx)`. On
  # failure it logs, appends a warning, and applies `on_failure` (default: keep
  # ctx unchanged, so the field retains its default) — letting a step install a
  # computed degraded value when the default isn't right (e.g. crash enrichment).
  defp stage(ctx, label, fun, on_failure \\ & &1) do
    fun.(ctx)
  rescue
    e ->
      Logger.warning("#{label} failed: #{Exception.format(:error, e, __STACKTRACE__)}")
      on_failure.(%{ctx | warnings: ctx.warnings ++ ["#{label} failed: #{Exception.message(e)}"]})
  end

  defp put_enriched(ctx, key, value), do: Map.update!(ctx, :enriched, &Map.put(&1, key, value))

  defp append_warnings(ctx, warnings), do: %{ctx | warnings: ctx.warnings ++ warnings}

  defp raw_crashes(ctx),
    do: Enum.map(ctx.snapshot.unexpected_crashes, &ResultBuilder.to_crash_event/1)

  # If crash enrichment fails wholesale, still hand attribution usable events:
  # the effective crash time falls back to the raw detection timestamp.
  defp fallback_enriched(raw_crashes),
    do: Enum.map(raw_crashes, &%{&1 | effective_at: &1.crash_info.timestamp})

  defp build_coredump_analyzer_opts(test_config) do
    opts = [timeout: test_config.coredump_timeout]

    case Toast.Diagnostics.Coredump.resolve_debugger(test_config.debugger) do
      nil -> opts
      debugger -> [{:debugger, debugger} | opts]
    end
  end

  # Flatten the per-deployment server maps into one keyed by server_id.
  # (server_ids are globally unique within a run)
  defp build_servers(snapshot) do
    snapshot.servers
    |> Map.values()
    |> Enum.reduce(%{}, &Map.merge/2)
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
