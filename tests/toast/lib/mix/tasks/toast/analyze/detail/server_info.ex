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

defmodule Mix.Tasks.Toast.Analyze.Detail.ServerInfo do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3]

  alias Mix.Tasks.Toast.Analyze.Data
  alias ToastTest.Formatting.Logs, as: LogFormatting
  alias ToastTest.Analyze.IssueStreams

  def print_context(issue, {win_start, win_end}, server_filter, servers, color) do
    {tb_start, tb_end} = issue.time_bounds
    matching = IssueStreams.matching_servers(servers, server_filter)
    deployments = issue[:deployments] || %{}

    Mix.shell().info(
      colorize(
        "  Issue window:  #{Data.fmt_dt(tb_start)} .. #{Data.fmt_dt(tb_end)}",
        :faint,
        color
      )
    )

    Mix.shell().info(
      colorize(
        "  Display window: #{Data.fmt_dt(win_start)} .. #{Data.fmt_dt(win_end)}",
        :faint,
        color
      )
    )

    Mix.shell().info(colorize("  Servers:", :faint, color))
    print_server_list(matching, servers, deployments, color)
  end

  defp print_server_list(matching, servers, deployments, color) do
    grouped = group_servers_by_deployment(matching, servers)

    if map_size(grouped) <= 1 do
      Enum.each(matching, fn server_id ->
        Mix.shell().info(colorize(format_server_label(server_id, servers), :faint, color))
      end)
    else
      print_grouped_server_list(grouped, servers, deployments, color)
    end
  end

  defp print_grouped_server_list(grouped, servers, deployments, color) do
    grouped
    |> Enum.sort_by(fn {did, _} -> did end)
    |> Enum.each(fn {did, server_ids} ->
      deployment_label = format_deployment_label(did, deployments)
      Mix.shell().info(colorize("    #{deployment_label}", :faint, color))

      Enum.each(server_ids, fn server_id ->
        Mix.shell().info(
          colorize(format_server_label(server_id, servers, "      "), :faint, color)
        )
      end)
    end)
  end

  defp group_servers_by_deployment(server_ids, servers) do
    Enum.group_by(server_ids, fn server_id ->
      case servers[server_id] do
        %{deployment_id: did} when is_binary(did) -> did
        _ -> "unknown"
      end
    end)
  end

  defp format_deployment_label(did, deployments) do
    case deployments[did] do
      %{mode: mode} when not is_nil(mode) -> "#{did} (#{mode})"
      _ -> did
    end
  end

  defp format_server_label(server_id, servers, indent \\ "    ") do
    meta = servers[server_id] || %{}
    tag = LogFormatting.server_tag(server_id, meta[:role])

    pid_part =
      case meta do
        %{incarnations: [%{pid: pid}]} ->
          "pid=#{pid}"

        %{incarnations: incs} when length(incs) > 1 ->
          "pids=#{Enum.map_join(incs, ",", &to_string(&1.pid))}"

        _ ->
          nil
      end

    [
      "#{indent}#{tag}  #{server_id}",
      if(match?(%{arango_id: id} when is_binary(id), meta), do: "arango=#{meta.arango_id}"),
      if(match?(%{endpoint: ep} when is_binary(ep), meta), do: "endpoint=#{meta.endpoint}"),
      pid_part
    ]
    |> Toast.Utils.compact()
    |> Enum.join("  ")
  end
end
