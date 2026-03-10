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

  @spec run(map(), ToastTest.ArtifactCollector.t(), term(), keyword()) ::
          [ToastTest.SuiteResult.issue()]
  def run(test_data, artifacts, error, opts \\ []) do
    windows = TimeWindows.build(test_data)

    test_failure_issues(test_data.failures) ++
      crash_issues(error, artifacts, windows, opts) ++
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

  defp crash_issues(nil, _artifacts, _windows, _opts), do: []

  defp crash_issues({:server_crashed, server_id, crash_info}, artifacts, windows, opts) do
    {scope, confidence} = TimeWindows.attribute(crash_info.timestamp, windows)
    server_artifacts = Map.get(artifacts, server_id)

    detail =
      %{server: server_id}
      |> enrich_coredump(server_artifacts, opts)
      |> enrich_logs(server_artifacts, crash_info.timestamp)

    [%{type: :crash, scope: scope, confidence: confidence, detail: detail}]
  end

  defp crash_issues({:server_unhealthy, server_id}, _artifacts, _windows, _opts) do
    [%{type: :crash, scope: :suite, confidence: nil, detail: %{server: server_id}}]
  end

  defp crash_issues(_other, _artifacts, _windows, _opts), do: []

  # --- Coredump enrichment ---

  defp enrich_coredump(detail, nil, _opts), do: detail
  defp enrich_coredump(detail, %{coredump_paths: []}, _opts), do: detail

  defp enrich_coredump(detail, server_artifacts, opts) do
    if Keyword.get(opts, :skip_coredump_analysis, false) do
      detail
    else
      core_path = hd(server_artifacts.coredump_paths)
      analyzer_opts = build_analyzer_opts(opts)

      case Enrichment.Coredump.analyze(core_path, server_artifacts.server, analyzer_opts) do
        {:ok, result} ->
          Map.merge(detail, %{
            signal: result.signal,
            threads: result.threads,
            coredump_path: core_path
          })

        {:error, _} ->
          detail
      end
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
