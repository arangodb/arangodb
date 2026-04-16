defmodule ToastTest.Runner.PostExecution do
  @moduledoc false

  alias ToastTest.{EventStore, SuiteResult}
  alias ToastTest.Formatting.{Color, Utils}
  alias ToastTest.Runner.ResultBuilder

  require Logger

  @spec run(Toast.Deployment.t() | nil, map(), ToastTest.Config.t()) :: SuiteResult.t()
  def run(nil, test_data, _test_config) do
    SuiteResult.build(test_data, [])
  end

  def run(deployment, test_data, %ToastTest.Config{} = test_config) do
    maybe_dump_agency(deployment, test_data, test_config)

    Logger.debug("Post-execution: stopping deployment")
    {servers, error} = stop_deployment(deployment)
    if error, do: Logger.warning("Deployment stop error: #{inspect(error)}")

    try do
      build_suite_result(servers, test_data, test_config)
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

  defp stop_deployment(deployment) do
    case Toast.Deployment.stop(deployment) do
      {:ok, info} -> {info.servers, info.error}
      {:error, _reason, info} -> {info.servers, info.error}
    end
  end

  defp build_suite_result(servers, test_data, test_config) do
    Utils.print_header("TEST EXECUTION FINISHED", IO.ANSI.enabled?(), Color.info())

    Logger.info("Running post-execution analysis...")

    snapshot = EventStore.snapshot()

    Logger.debug("Collecting artifacts")
    artifact_opts = [coredump_dir: test_config.coredump_dir, not_before: test_data.started_at]

    artifacts =
      ToastTest.ArtifactCollector.collect(servers, snapshot.pids_by_server, artifact_opts)

    Logger.debug("Running attribution")

    crash_events = Enum.map(snapshot.unexpected_crashes, &ResultBuilder.to_crash_event/1)
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

    Logger.debug("Building results (#{length(issues)} issues found)")
    active_sanitizers = Application.get_env(:toast, :active_sanitizers, MapSet.new())

    warnings =
      ResultBuilder.coredump_warnings(
        crash_events,
        artifacts,
        test_config.coredump_dir,
        active_sanitizers
      )

    deployments = ResultBuilder.build_deployments(snapshot, server_logs)

    suite_result =
      SuiteResult.build(test_data, issues,
        warnings: warnings,
        deployments: deployments,
        coredumps: coredump_reports,
        events: snapshot.events
      )

    SuiteResult.write_all(suite_result, test_config.result_dir)
    print_post_exec_summary(suite_result)
    suite_result
  end

  defp build_coredump_analyzer_opts(test_config) do
    opts = [timeout: test_config.coredump_timeout]

    case Toast.Diagnostics.Coredump.resolve_debugger(test_config.debugger) do
      nil -> opts
      debugger -> [{:debugger, debugger} | opts]
    end
  end

  defp maybe_dump_agency(deployment, test_data, test_config) do
    has_error =
      ToastTest.Abort.reason() != nil or
        EventStore.unexpected_crashes() != [] or
        test_data.failures != []

    if has_error and test_config.dump_agency_on_error do
      case Toast.Deployment.dump_agency(deployment) do
        {:ok, json} when json != nil ->
          write_agency_dump(json, test_config.result_dir, deployment.id)

        {:ok, nil} ->
          Logger.warning("Agency dump returned nil (no responsive agents?)")

        {:error, reason} ->
          Logger.debug("Agency dump skipped: #{inspect(reason)}")
      end
    end
  rescue
    e ->
      Logger.warning("Agency dump failed: #{Exception.message(e)}")
  end

  defp write_agency_dump(json, result_dir, deployment_id) do
    case Toast.Diagnostics.AgencyDump.write(json, result_dir, deployment_id) do
      {:ok, path} ->
        Logger.info("Agency dump written to #{path}")

      {:error, reason} ->
        Logger.warning("Failed to write agency dump: #{inspect(reason)}")
    end
  end

  defp print_post_exec_summary(suite_result) do
    ToastTest.Formatting.PostExecSummary.print(suite_result)
  end
end
