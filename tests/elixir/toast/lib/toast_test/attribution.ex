defmodule ToastTest.Attribution do
  @moduledoc """
  Orchestrates issue production from test data, artifacts, and deployment errors.

  Combines test failures, crash analysis, and sanitizer reports into a flat
  list of `SuiteResult.issue()` maps.
  """

  alias ToastTest.Attribution.TimeWindows
  alias ToastTest.Enrichment

  @crash_log_window_before_s 10
  @crash_log_window_after_s 5

  @spec run(
          ToastTest.ResultCollector.test_data(),
          ToastTest.ArtifactCollector.t(),
          [Toast.Process.CrashEvent.t()],
          keyword()
        ) ::
          [ToastTest.SuiteResult.issue()]
  def run(test_data, artifacts, crash_events, opts \\ []) do
    windows = TimeWindows.build(test_data)

    test_failure_issues(test_data.failures) ++
      crash_issues(crash_events, artifacts, windows, opts) ++
      sanitizer_issues(artifacts, windows)
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
    Enum.flat_map(crash_events, fn %Toast.Process.CrashEvent{} = event ->
      {scope, confidence} = TimeWindows.attribute(event.crash_info.timestamp, windows)
      server_artifacts = Map.get(artifacts, event.server_id)

      detail =
        %{server: event.server_id}
        |> enrich_coredumps(server_artifacts, opts)
        |> enrich_logs(server_artifacts, event.crash_info.timestamp)

      [%{type: :crash, scope: scope, confidence: confidence, detail: detail}]
    end)
  end

  # --- Coredump enrichment ---

  defp enrich_coredumps(detail, nil, _opts), do: detail
  defp enrich_coredumps(detail, %{coredump_paths: []}, _opts), do: detail

  defp enrich_coredumps(detail, server_artifacts, opts) do
    if Keyword.get(opts, :skip_coredump_analysis, false) do
      Map.put(detail, :coredump_paths, server_artifacts.coredump_paths)
    else
      analyzer_opts = build_analyzer_opts(opts)

      coredumps =
        Enum.flat_map(server_artifacts.coredump_paths, fn core_path ->
          case Enrichment.Coredump.analyze(core_path, server_artifacts.server, analyzer_opts) do
            {:ok, result} ->
              [%{path: core_path, signal: result.signal, threads: result.threads}]

            {:error, _} ->
              [%{path: core_path, signal: nil, threads: []}]
          end
        end)

      Map.put(detail, :coredumps, coredumps)
    end
  end

  defp build_analyzer_opts(opts) do
    case Keyword.get(opts, :coredump_analyzer) do
      nil -> []
      analyzer -> [analyzer: analyzer]
    end
  end

  # --- Log enrichment ---

  defp enrich_logs(detail, nil, _timestamp), do: detail

  defp enrich_logs(detail, server_artifacts, timestamp) do
    log_file = server_artifacts.server.log_file

    if log_file do
      start_dt = DateTime.add(timestamp, -@crash_log_window_before_s, :second)
      end_dt = DateTime.add(timestamp, @crash_log_window_after_s, :second)
      logs = Enrichment.Logs.extract_window(log_file, start_dt, end_dt)

      if logs != "", do: Map.put(detail, :logs, logs), else: detail
    else
      detail
    end
  end

  # --- Sanitizer reports ---

  defp sanitizer_issues(artifacts, windows) do
    Enum.flat_map(artifacts, fn {server_id, server_artifacts} ->
      Enum.flat_map(server_artifacts.sanitizer_files, fn san_file ->
        case Enrichment.Sanitizer.read(san_file) do
          {:ok, result} ->
            {scope, confidence} = TimeWindows.attribute(result.timestamp, windows)

            [
              %{
                type: :sanitizer_report,
                scope: scope,
                confidence: confidence,
                detail: %{server: server_id, report: result.content}
              }
            ]

          {:error, _} ->
            []
        end
      end)
    end)
  end
end
