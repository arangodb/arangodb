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

defmodule ToastTest.Enrichment do
  @moduledoc """
  The post-execution enrichment phase: turn discovered artifact *paths* into
  parsed *data*, exactly once, before the pure attribution step.

  This is the impure boundary between `ArtifactCollector` (discovers paths,
  reads nothing) and `ToastTest.Attribution` (decides over data, reads
  nothing). It uses the parser submodules `Enrichment.Logs`,
  `Enrichment.Coredump`, and `Enrichment.Sanitizer`.

  Each unit (per crashed server, per sanitizer file) degrades independently:
  a failure contributes a warning and leaves the other units intact, so a
  single unreadable log or a hung debugger never blanks the whole diagnostics
  output.
  """

  alias ToastTest.CrashEvent
  alias ToastTest.Enrichment

  require Logger

  @doc """
  Enrich crash events with filesystem data: the log-resolved crash timestamp
  (`effective_at`), the trailing crash log lines, the originating log file, and
  analyzed coredump reports.

  `crash_info.timestamp` is never rewritten — it stays the raw process-exit
  detection time. `analyzer_opts` are forwarded to `Enrichment.Coredump.analyze/3`
  (and may carry an `:analyzer` override for testing).

  Returns `{enriched_events, warnings}`.
  """
  @spec enrich_crashes([CrashEvent.t()], ToastTest.ArtifactCollector.t(), keyword()) ::
          {[CrashEvent.t()], [String.t()]}
  def enrich_crashes(crash_events, artifacts, analyzer_opts \\ []) do
    degrade_each(
      crash_events,
      &enrich_crash(&1, artifacts, analyzer_opts),
      fn event, e ->
        {%{event | effective_at: event.crash_info.timestamp},
         "Crash enrichment failed for #{event.server_id}: #{Exception.message(e)}"}
      end
    )
  end

  @doc """
  Read and parse every server's sanitizer report files into plain data.

  Each result is `%{server_id, file, content, timestamp, kind}`; a single file
  may yield several reports. `timestamp` is `nil` when no matching sidecar makes
  the report attributable. A file that fails to read contributes a warning and
  is skipped.

  Returns `{reports, warnings}`.
  """
  @spec sanitizer_reports(ToastTest.ArtifactCollector.t()) :: {[map()], [String.t()]}
  def sanitizer_reports(artifacts) do
    files =
      for {server_id, %{sanitizer_files: sanitizer_files}} <- artifacts,
          file <- sanitizer_files,
          do: {server_id, file}

    {nested, warnings} =
      degrade_each(
        files,
        fn {server_id, file} -> read_sanitizer_file(server_id, file) end,
        fn {_server_id, file}, e ->
          {[], "Sanitizer enrichment failed for #{file}: #{Exception.message(e)}"}
        end
      )

    {List.flatten(nested), warnings}
  end

  # Map `fun` over `items`, degrading each unit independently: if `fun.(item)`
  # raises, `recover.(item, exception)` supplies a `{fallback, warning}` pair so
  # one bad unit never blanks the others. Returns `{results, warnings}`.
  defp degrade_each(items, fun, recover) do
    {results, warnings} =
      Enum.map_reduce(items, [], fn item, warns ->
        try do
          {fun.(item), warns}
        rescue
          e ->
            {fallback, warning} = recover.(item, e)
            Logger.warning(warning)
            {fallback, [warning | warns]}
        end
      end)

    {results, Enum.reverse(warnings)}
  end

  @doc """
  Attach the discovered coredump path to each server named by a timeout kill,
  so attribution stays pure over enriched data rather than reaching back into
  the raw artifact inventory.
  """
  @spec enrich_timeout_kills([map()], ToastTest.ArtifactCollector.t()) :: [map()]
  def enrich_timeout_kills(timeout_kills, artifacts) do
    Enum.map(timeout_kills, fn kill ->
      servers =
        Enum.map(kill.servers, fn server ->
          Map.put(server, :coredump, first_coredump_path(artifacts, server.server_id))
        end)

      %{kill | servers: servers}
    end)
  end

  defp first_coredump_path(artifacts, server_id) do
    case Map.get(artifacts, server_id) do
      %{coredump_paths: [path | _]} -> path
      _ -> nil
    end
  end

  defp read_sanitizer_file(server_id, file) do
    sidecar = Enrichment.Sanitizer.sidecar_path_for(file)

    case Enrichment.Sanitizer.read_all(file, sidecar) do
      {:ok, results} ->
        Enum.map(results, fn result ->
          %{
            server_id: server_id,
            file: file,
            content: result.content,
            timestamp: result.timestamp,
            kind: result.kind
          }
        end)

      {:error, reason} ->
        Logger.warning("Sanitizer file #{Path.basename(file)}: read failed (#{inspect(reason)})")
        []
    end
  end

  defp enrich_crash(%CrashEvent{} = event, artifacts, analyzer_opts) do
    server_artifacts = Map.get(artifacts, event.server_id)
    {crash_entries, log_file} = extract_server_errors(server_artifacts)

    %{
      event
      | effective_at: extract_crash_timestamp(crash_entries) || event.crash_info.timestamp,
        crash_lines: format_crash_entries(crash_entries),
        log_file: log_file,
        coredump_reports:
          analyze_coredumps(
            event.server_id,
            filter_artifacts_by_pid(server_artifacts, event.crash_info.os_pid),
            event.crash_info.executable,
            analyzer_opts
          )
    }
  end

  # --- Coredump analysis ---

  defp analyze_coredumps(_server_id, nil, _executable, _opts), do: []
  defp analyze_coredumps(_server_id, %{coredump_paths: []}, _executable, _opts), do: []

  defp analyze_coredumps(server_id, server_artifacts, executable, analyzer_opts) do
    paths = server_artifacts.coredump_paths
    Logger.info("Analyzing #{length(paths)} coredump(s) for server #{server_id}")

    Enum.flat_map(paths, fn core_path ->
      case Enrichment.Coredump.analyze(core_path, executable, analyzer_opts) do
        {:ok, result} ->
          Logger.info("Coredump #{Path.basename(core_path)}: #{length(result.threads)} thread(s)")

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
  end

  defp filter_artifacts_by_pid(nil, _os_pid), do: nil
  defp filter_artifacts_by_pid(artifacts, nil), do: artifacts

  defp filter_artifacts_by_pid(artifacts, os_pid) do
    pid_str = to_string(os_pid)

    artifacts.coredump_paths
    |> Enum.filter(fn path ->
      path |> Path.basename() |> String.split(~r/[.\-_]/) |> Enum.member?(pid_str)
    end)
    |> case do
      # Fall back to all paths if none matched (e.g., core files without PID in name)
      [] -> artifacts
      paths -> %{artifacts | coredump_paths: paths}
    end
  end

  # --- Crash log helpers ---

  defp extract_server_errors(nil), do: {[], nil}
  defp extract_server_errors(%{log_file: nil}), do: {[], nil}

  defp extract_server_errors(%{log_file: log_file}) do
    {Enrichment.Logs.extract_trailing_errors(log_file), log_file}
  end

  defp extract_crash_timestamp(entries) do
    case Enum.find(entries, &(&1[:topic] == :crash)) do
      %{time: time} -> time
      nil -> nil
    end
  end

  defp format_crash_entries([]), do: nil

  defp format_crash_entries(entries) do
    Enum.map_join(entries, "\n", fn entry ->
      level = entry[:level] |> to_string() |> String.upcase()
      topic = if entry[:topic], do: " {#{entry.topic}}", else: ""
      "[#{level}]#{topic} #{entry.message}"
    end)
  end
end
