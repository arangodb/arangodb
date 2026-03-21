defmodule ToastTest.Attribution do
  @moduledoc """
  Orchestrates issue production from test data, artifacts, and deployment errors.

  Combines test failures, crash analysis, and sanitizer reports into a flat
  list of `SuiteResult.issue()` maps.
  """

  alias ToastTest.Attribution.TimeWindows
  alias ToastTest.Enrichment

  require Logger

  @spec run(
          ToastTest.ResultCollector.test_data(),
          ToastTest.ArtifactCollector.t(),
          [Toast.Process.CrashEvent.t()],
          keyword()
        ) ::
          [ToastTest.SuiteResult.issue()]
  def run(test_data, artifacts, crash_events, opts \\ []) do
    windows = TimeWindows.build(test_data)
    timeout_kills = Keyword.get(opts, :timeout_kills, [])

    issues =
      test_failure_issues(test_data.failures) ++
        crash_issues(crash_events, artifacts, windows, opts) ++
        sanitizer_issues(artifacts, windows) ++
        timeout_issues(timeout_kills, artifacts)

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

  defp crash_issues(crash_events, artifacts, windows, opts) do
    analyzer_opts = Keyword.get(opts, :analyzer_opts, [])

    Enum.map(crash_events, fn %Toast.Process.CrashEvent{} = event ->
      crash_dt = DateTime.from_unix!(event.crash_info.timestamp, :microsecond)
      {scope, confidence} = TimeWindows.attribute(crash_dt, windows)
      server_artifacts = Map.get(artifacts, event.server_id)

      detail =
        %{server: event.server_id, crash_info: event.crash_info}
        |> enrich_coredumps(server_artifacts, analyzer_opts)
        |> enrich_logs(server_artifacts, event.crash_info.timestamp)

      %{type: :crash, scope: scope, confidence: confidence, detail: detail}
    end)
  end

  # --- Coredump enrichment ---

  defp enrich_coredumps(detail, nil, _analyzer_opts), do: detail
  defp enrich_coredumps(detail, %{coredump_paths: []}, _analyzer_opts), do: detail

  defp enrich_coredumps(detail, server_artifacts, analyzer_opts) do
    paths = server_artifacts.coredump_paths

    Logger.info("Enriching #{length(paths)} coredump(s) for server #{detail.server}")

    coredumps =
      Enum.flat_map(paths, fn core_path ->
        analyze_coredump(core_path, server_artifacts.server, analyzer_opts)
      end)

    Map.put(detail, :coredumps, coredumps)
  end

  defp analyze_coredump(core_path, server, analyzer_opts) do
    case Enrichment.Coredump.analyze(core_path, server, analyzer_opts) do
      {:ok, result} ->
        Logger.info("Coredump #{Path.basename(core_path)}: #{length(result.threads)} thread(s)")
        [%{path: core_path, signal: result.signal, threads: result.threads}]

      {:error, reason} ->
        Logger.warning(
          "Coredump #{Path.basename(core_path)}: analysis failed (#{inspect(reason)})"
        )

        [%{path: core_path, signal: nil, threads: []}]
    end
  end

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
        {:ok, result} <- [Enrichment.Sanitizer.read(san_file)] do
      {scope, confidence} = TimeWindows.attribute(result.timestamp, windows)

      %{
        type: :sanitizer_report,
        scope: scope,
        confidence: confidence,
        detail: %{
          server: server_id,
          report: result.content,
          timestamp: result.timestamp,
          kind: result.kind
        }
      }
    end
  end
end
