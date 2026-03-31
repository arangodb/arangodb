defmodule ToastTest.Runner.PostExecution do
  @moduledoc false

  alias ToastTest.{EventStore, SuiteResult}

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
    snapshot = EventStore.snapshot()

    Logger.debug("Collecting artifacts")
    artifact_opts = [coredump_dir: test_config.coredump_dir, not_before: test_data.started_at]

    artifacts =
      ToastTest.ArtifactCollector.collect(servers, snapshot.pids_by_server, artifact_opts)

    Logger.debug("Running attribution")

    crash_events = Enum.map(snapshot.unexpected_crashes, &to_crash_event/1)

    {issues, coredump_reports} =
      ToastTest.Attribution.run(test_data, artifacts, crash_events,
        timeout_kills: snapshot.timeout_kills,
        analyzer_opts: build_coredump_analyzer_opts(test_config)
      )

    Logger.debug("Collecting server logs")
    windows = ToastTest.Attribution.TimeWindows.build(test_data)
    all_log_files = collect_log_files(snapshot.servers)
    server_logs = ToastTest.Attribution.ServerLogs.collect(issues, all_log_files, windows)

    Logger.debug("Building results (#{length(issues)} issues found)")
    warnings = coredump_warnings(crash_events, artifacts, test_config)
    deployments = build_deployments(snapshot, server_logs)

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

  defp build_deployments(snapshot, server_logs) do
    Map.new(snapshot.deployments, fn {did, deployment_info} ->
      servers_with_logs =
        Map.new(Map.get(snapshot.servers, did, %{}), fn {sid, server} ->
          {sid, Map.put(server, :logs, Map.get(server_logs, sid, []))}
        end)

      {did,
       %{
         id: did,
         mode: deployment_info.mode,
         stacktrace: deployment_info.stacktrace,
         started_at: deployment_info.started_at,
         stopped_at: deployment_info.stopped_at,
         servers: servers_with_logs
       }}
    end)
  end

  defp collect_log_files(servers_by_deployment) do
    for {_did, servers} <- servers_by_deployment,
        {sid, server} <- servers,
        log_file = server[:log_file],
        log_file != nil,
        into: %{} do
      {sid, log_file}
    end
  end

  defp to_crash_event(%{server_id: sid, crash_info: info} = e) do
    %Toast.Process.CrashEvent{
      server_id: sid,
      crash_info: info,
      expected: Map.get(e, :expected, false)
    }
  end

  defp coredump_warnings(crash_events, artifacts, test_config) do
    if crash_events != [] and not ToastTest.ArtifactCollector.has_coredumps?(artifacts) do
      [
        sanitizer_coredump_warning(),
        Toast.Diagnostics.Coredump.coredump_discovery_warning(test_config.coredump_dir)
      ]
      |> Toast.Utils.compact()
    else
      []
    end
  end

  defp sanitizer_coredump_warning do
    active = Application.get_env(:toast, :active_sanitizers, MapSet.new())

    if MapSet.size(active) > 0,
      do:
        "Sanitizer build detected — coredumps are typically not generated with sanitizers enabled"
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
          case Toast.Diagnostics.AgencyDump.write(
                 json,
                 test_config.result_dir,
                 deployment.id
               ) do
            {:ok, path} ->
              Logger.info("Agency dump written to #{path}")

            {:error, reason} ->
              Logger.warning("Failed to write agency dump: #{inspect(reason)}")
          end

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

  defp print_post_exec_summary(suite_result) do
    ToastTest.PostExecSummary.print(suite_result)
  end
end
