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

defmodule ToastTest.Analyze.AgencyLogs do
  @moduledoc """
  Window-filtering of stored agency-dump log excerpts for display.

  The analyze counterpart of `ToastTest.PostExecution.Attribution.AgencyLogs`,
  which collects the excerpts. This module narrows those excerpts to a single
  display window and tags each deployment's entries for stream merging, the
  way `ToastTest.Analyze.Logs` does for server logs.
  """

  @type windowed_entries :: {Toast.timestamp(), Toast.timestamp(), [map()]}

  @doc """
  Filter stored agency log excerpts by the given display window.

  `agency_logs` is `%{deployment_id => [{start, end, [entry]}]}` as stored in
  `SuiteResult.agency_logs`. Returns `[{{:agency, deployment_id}, [entry]}]`
  for deployments with at least one entry in the window, sorted by deployment
  id; each deployment's entries are sorted by time. Each entry keeps its
  agency-dump `"time_us"` and gains a `:timestamp` key so the shared stream
  merge (`IssueStreams.merge/1`) can order it like every other stream.
  """
  @spec extract(%{String.t() => [windowed_entries()]}, {Toast.timestamp(), Toast.timestamp()}) ::
          [{{:agency, String.t()}, [map()]}]
  def extract(agency_logs, {start_us, end_us}) do
    agency_logs
    |> Enum.flat_map(fn {deployment_id, chunks} ->
      entries =
        chunks
        |> Enum.flat_map(fn {_start, _end, entries} -> entries end)
        |> Enum.filter(&in_window?(&1, start_us, end_us))
        |> Enum.sort_by(& &1["time_us"])
        |> Enum.map(&Map.put(&1, :timestamp, &1["time_us"]))

      if entries == [], do: [], else: [{{:agency, deployment_id}, entries}]
    end)
    |> Enum.sort_by(fn {{:agency, deployment_id}, _} -> deployment_id end)
  end

  defp in_window?(%{"time_us" => ts}, start_us, end_us), do: ts >= start_us and ts <= end_us
  defp in_window?(_entry, _start_us, _end_us), do: false
end
