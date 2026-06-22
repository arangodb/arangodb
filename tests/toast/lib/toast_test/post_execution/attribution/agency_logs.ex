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

defmodule ToastTest.PostExecution.Attribution.AgencyLogs do
  @moduledoc """
  Extracts agency-dump log excerpts that fall within issue time windows.

  Reuses the same per-issue windows as `ServerLogs` — the agency log is
  cluster-wide context for the same events — then keeps each dump's log
  entries whose timestamps land inside those windows. Mirrors `ServerLogs`:
  the result is keyed by source (here, deployment id) so logs from different
  deployments' agencies stay distinct.
  """

  alias Toast.Diagnostics.AgencyDump
  alias ToastTest.PostExecution.Attribution.ServerLogs

  require Logger

  @type windowed_entries :: {Toast.timestamp(), Toast.timestamp(), [map()]}

  @doc """
  Collect agency log excerpts for the given issues.

  `agency_dumps` is `%{deployment_id => dump_path}`. Returns
  `%{deployment_id => [{start_us, end_us, entries}]}` — one entry per merged
  issue window containing matching entries; returns `%{}` when there are no
  windows or no dumps.
  """
  @spec collect(
          [ToastTest.SuiteResult.issue()],
          %{String.t() => Path.t()},
          ToastTest.TimeWindows.windows()
        ) :: %{String.t() => [windowed_entries()]}
  def collect(issues, agency_dumps, windows) do
    issues
    |> ServerLogs.compute_windows(windows)
    |> ServerLogs.merge_windows()
    |> do_collect(agency_dumps)
  end

  defp do_collect([], _agency_dumps), do: %{}

  defp do_collect(merged, agency_dumps) do
    Map.new(agency_dumps, fn {deployment_id, path} ->
      {deployment_id, path |> read_entries(deployment_id) |> window_entries(merged)}
    end)
  end

  defp read_entries(path, deployment_id) do
    case AgencyDump.extract_log_entries_from_file(path) do
      {:ok, entries} ->
        entries

      {:error, reason} ->
        Logger.warning("Failed to extract agency log for #{deployment_id}: #{inspect(reason)}")
        []
    end
  end

  defp window_entries(entries, merged) do
    Enum.flat_map(merged, fn {start_us, end_us} ->
      case Enum.filter(entries, &in_window?(&1, start_us, end_us)) do
        [] -> []
        matching -> [{start_us, end_us, matching}]
      end
    end)
  end

  defp in_window?(%{"time_us" => ts}, start_us, end_us), do: ts >= start_us and ts <= end_us
end
