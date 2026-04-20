defmodule ToastTest.Runner.ResultBuilder do
  @moduledoc """
  Transforms raw post-execution data into structured results.

  Builds deployment summaries, collects log file paths, converts crash maps
  to typed structs, and generates coredump-related warnings.
  """

  @spec build_deployments(map(), %{optional(String.t()) => [map()]}) :: %{
          optional(String.t()) => map()
        }
  def build_deployments(snapshot, server_logs) do
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

  @spec collect_log_files(map()) :: %{optional(String.t()) => String.t()}
  def collect_log_files(servers_by_deployment) do
    for {_did, servers} <- servers_by_deployment,
        {sid, server} <- servers,
        log_file = server[:log_file],
        log_file != nil,
        into: %{} do
      {sid, log_file}
    end
  end

  @spec to_crash_event(map()) :: ToastTest.CrashEvent.t()
  def to_crash_event(%{server_id: sid, crash_info: info} = e) do
    %ToastTest.CrashEvent{
      server_id: sid,
      crash_info: info,
      expected: Map.get(e, :expected, false)
    }
  end

  @spec coredump_warnings([map()], map(), String.t() | nil, MapSet.t()) :: [String.t()]
  def coredump_warnings(crash_events, artifacts, coredump_dir, active_sanitizers) do
    if crash_events != [] and not ToastTest.ArtifactCollector.has_coredumps?(artifacts) do
      [
        sanitizer_coredump_warning(active_sanitizers),
        Toast.Diagnostics.Coredump.Discovery.coredump_discovery_warning(coredump_dir)
      ]
      |> Toast.Utils.compact()
    else
      []
    end
  end

  defp sanitizer_coredump_warning(active_sanitizers) do
    if MapSet.size(active_sanitizers) > 0,
      do:
        "Sanitizer build detected — coredumps are typically not generated with sanitizers enabled"
  end
end
