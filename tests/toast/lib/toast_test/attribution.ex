defmodule ToastTest.Attribution do
  @moduledoc """
  Orchestrates issue production from test data, artifacts, and deployment errors.

  Combines test failures, crash analysis, and sanitizer reports into a flat
  list of `SuiteResult.issue()` maps.
  """

  alias ToastTest.Attribution.TimeWindows
  alias ToastTest.Enrichment

  import Toast.Utils, only: [maybe_put: 3]

  require Logger

  @spec run(
          ToastTest.ResultCollector.test_data(),
          ToastTest.ArtifactCollector.t(),
          [ToastTest.CrashEvent.t()],
          keyword()
        ) ::
          {[ToastTest.SuiteResult.issue()], [ToastTest.SuiteResult.coredump_report()]}
  def run(test_data, artifacts, crash_events, opts \\ []) do
    windows = TimeWindows.build(test_data)
    timeout_kills = Keyword.get(opts, :timeout_kills, [])

    {crash_issues, coredump_reports} =
      crash_issues(crash_events, artifacts, windows, opts)

    issues =
      test_failure_issues(test_data.failures) ++
        crash_issues ++
        sanitizer_issues(artifacts, windows) ++
        timeout_issues(timeout_kills, artifacts)

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

  defp crash_issues(crash_events, artifacts, windows, opts) do
    analyzer_opts = Keyword.get(opts, :analyzer_opts, [])

    {issues, coredump_reports} =
      Enum.reduce(crash_events, {[], []}, fn event, {issues_acc, dumps_acc} ->
        {scope, confidence, phase} = TimeWindows.attribute(event.crash_info.timestamp, windows)
        server_artifacts = Map.get(artifacts, event.server_id)

        {coredump_paths, new_dumps} =
          analyze_coredumps(
            event.server_id,
            filter_artifacts_by_pid(server_artifacts, event.crash_info.os_pid),
            analyzer_opts
          )

        detail =
          %{server: event.server_id, crash_info: event.crash_info}
          |> maybe_put(:phase, phase)
          |> maybe_put_coredump_paths(coredump_paths)
          |> enrich_logs(server_artifacts, event.crash_info.timestamp)

        issue = %{type: :crash, scope: scope, confidence: confidence, detail: detail}
        {[issue | issues_acc], new_dumps ++ dumps_acc}
      end)

    {Enum.reverse(issues), Enum.reverse(coredump_reports)}
  end

  # --- Coredump analysis ---

  defp analyze_coredumps(_server_id, nil, _opts), do: {[], []}
  defp analyze_coredumps(_server_id, %{coredump_paths: []}, _opts), do: {[], []}

  defp analyze_coredumps(server_id, server_artifacts, analyzer_opts) do
    paths = server_artifacts.coredump_paths
    Logger.info("Analyzing #{length(paths)} coredump(s) for server #{server_id}")

    reports =
      Enum.flat_map(paths, fn core_path ->
        case Enrichment.Coredump.analyze(core_path, server_artifacts.server, analyzer_opts) do
          {:ok, result} ->
            Logger.info(
              "Coredump #{Path.basename(core_path)}: #{length(result.threads)} thread(s)"
            )

            [
              %{
                core_path: core_path,
                server_id: server_id,
                debugger: result.debugger,
                signal: result.signal,
                faulting_address: result.faulting_address,
                registers: result.registers,
                disassembly: result.disassembly,
                crash_thread: result.crash_thread,
                threads: result.threads
              }
            ]

          {:error, reason} ->
            Logger.warning(
              "Coredump #{Path.basename(core_path)}: analysis failed (#{inspect(reason)})"
            )

            []
        end
      end)

    coredump_paths = Enum.map(reports, & &1.core_path)
    {coredump_paths, reports}
  end

  defp filter_artifacts_by_pid(nil, _os_pid), do: nil
  defp filter_artifacts_by_pid(artifacts, nil), do: artifacts

  defp filter_artifacts_by_pid(artifacts, os_pid) do
    pid_str = to_string(os_pid)

    Enum.filter(artifacts.coredump_paths, fn path ->
      path |> Path.basename() |> String.split(~r/[.\-_]/) |> Enum.member?(pid_str)
    end)
    |> case do
      # Fall back to all paths if none matched (e.g., core files without PID in name)
      [] -> artifacts
      paths -> %{artifacts | coredump_paths: paths}
    end
  end

  defp maybe_put_coredump_paths(detail, []), do: detail
  defp maybe_put_coredump_paths(detail, paths), do: Map.put(detail, :coredump_paths, paths)

  # --- Log enrichment ---

  defp enrich_logs(detail, nil, _timestamp), do: detail

  defp enrich_logs(detail, %{server: %{log_file: nil}}, _timestamp), do: detail

  defp enrich_logs(detail, %{server: %{log_file: log_file}}, _timestamp) do
    detail = Map.put(detail, :log_file, log_file)

    # Extract only the last contiguous {crash} block — earlier crashes in the
    # same log (e.g. from resilience tests) are expected and irrelevant.
    case Enrichment.Logs.extract_crash_lines(log_file) do
      [] -> detail
      entries -> Map.put(detail, :crash_lines, format_crash_entries(entries))
    end
  end

  defp format_crash_entries(entries) do
    entries
    |> Enum.map_join("\n", fn entry ->
      level = entry[:level] |> to_string() |> String.upcase()
      topic = if entry[:topic], do: " {#{entry.topic}}", else: ""
      "[#{level}]#{topic} #{entry.message}"
    end)
  end

  # --- Timeouts ---

  defp timeout_issues([], _artifacts), do: []

  defp timeout_issues(timeout_kills, artifacts) do
    Enum.map(timeout_kills, fn kill ->
      servers =
        Enum.map(kill.servers, fn server_info ->
          coredump_path = find_coredump_path(artifacts, server_info.server_id)
          Map.put(server_info, :coredump, coredump_path)
        end)

      %{
        type: :timeout,
        scope: :suite,
        confidence: :high,
        detail: %{
          source: kill.source,
          reason: kill.reason,
          timestamp: kill.timestamp,
          servers: servers
        }
      }
    end)
  end

  defp find_coredump_path(artifacts, server_id) do
    case Map.get(artifacts, server_id) do
      %{coredump_paths: [path | _]} -> path
      _ -> nil
    end
  end

  # --- Sanitizer reports ---

  defp sanitizer_issues(artifacts, windows) do
    for {server_id, server_artifacts} <- artifacts,
        san_file <- server_artifacts.sanitizer_files,
        sidecar = Enrichment.Sanitizer.sidecar_path_for(san_file),
        {:ok, results} <- [Enrichment.Sanitizer.read_all(san_file, sidecar)],
        result <- results do
      {scope, confidence, phase} =
        if is_integer(result.timestamp) do
          TimeWindows.attribute(result.timestamp, windows)
        else
          {:suite, nil, nil}
        end

      detail =
        %{
          server: server_id,
          file: san_file,
          report: result.content,
          timestamp: result.timestamp,
          kind: result.kind
        }
        |> maybe_put(:phase, phase)

      %{type: :sanitizer_report, scope: scope, confidence: confidence, detail: detail}
    end
  end
end
