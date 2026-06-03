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

defmodule Mix.Tasks.Toast.Analyze.Detail.Streams do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3]

  alias Mix.Tasks.Toast.Analyze.Detail.ServerInfo
  alias ToastTest.Formatting.Logs, as: LogFormatting
  alias ToastTest.Analyze.IssueStreams
  alias ToastTest.Analyze.Logs, as: LogAnalysis
  alias ToastTest.Traffic.Analysis, as: TrafficAnalysis

  def print_logs(issue, log_opts, color) do
    servers = issue[:servers] || %{}
    window = IssueStreams.display_window(issue, log_opts.window_spec)
    bar = String.duplicate("─", 50)
    Mix.shell().info("\n#{colorize("── Server logs " <> bar, :faint, color)}")

    case window do
      nil ->
        Mix.shell().info(colorize("  No time bounds available for this issue.", :faint, color))

      {win_start, win_end} = window ->
        print_log_window(issue, window, win_start, win_end, log_opts, servers, color)
    end
  end

  def print_traffic(issue, traffic_opts, color) do
    window = IssueStreams.display_window(issue, traffic_opts.window_spec)
    bar = String.duplicate("─", 50)
    Mix.shell().info("\n#{colorize("── Traffic " <> bar, :faint, color)}")

    case window do
      nil ->
        Mix.shell().info(colorize("  No time bounds available for this issue.", :faint, color))

      window ->
        server_ports = build_server_ports(issue[:deployments] || %{})
        entries = extract_traffic_entries(issue, window, traffic_opts, server_ports)

        if entries == [] do
          Mix.shell().info(colorize("  No matching traffic entries.", :faint, color))
        else
          Mix.shell().info("")
          print_traffic_entries(entries, server_ports, color)
        end
    end
  end

  def print_logs_and_traffic(issue, log_opts, traffic_opts, color) do
    servers = issue[:servers] || %{}
    window = IssueStreams.display_window(issue, log_opts.window_spec)
    bar = String.duplicate("─", 50)
    Mix.shell().info("\n#{colorize("── Logs + Traffic " <> bar, :faint, color)}")

    case window do
      nil ->
        Mix.shell().info(colorize("  No time bounds available for this issue.", :faint, color))

      {win_start, win_end} = window ->
        ServerInfo.print_context(issue, win_start, win_end, log_opts, servers, color)

        {log_entries, events} = prepare_log_data(issue, window, log_opts, servers)

        server_ports = build_server_ports(issue[:deployments] || %{})
        traffic_entries = extract_traffic_entries(issue, window, traffic_opts, server_ports)

        print_merged(log_entries, events, traffic_entries, log_opts, servers, server_ports, color)
    end
  end

  defp print_merged(log_entries, events, traffic_entries, log_opts, servers, server_ports, color) do
    if log_entries == [] and events == [] and traffic_entries == [] do
      Mix.shell().info(colorize("  No matching entries.", :faint, color))
    else
      Mix.shell().info("")
      server_roles = Map.new(servers, fn {sid, meta} -> {sid, meta[:role]} end)

      event_stream = if events != [], do: [{:event, events}], else: []
      traffic_stream = if traffic_entries != [], do: [{:traffic, traffic_entries}], else: []
      merged = IssueStreams.merge(log_entries ++ event_stream ++ traffic_stream)

      output =
        Enum.map_join(merged, "\n", fn
          {:event, event} ->
            format_event_line(event, log_opts.event_detail)

          {:traffic, entry} ->
            format_traffic_line(entry, server_ports, color)

          {server_id, log_entry} ->
            format_log_line(server_id, log_entry, server_roles, color)
        end)

      Mix.shell().info(output)
    end
  end

  defp print_log_window(issue, window, win_start, win_end, log_opts, servers, color) do
    ServerInfo.print_context(issue, win_start, win_end, log_opts, servers, color)

    {entries, events} = prepare_log_data(issue, window, log_opts, servers)

    if entries == [] and events == [] do
      Mix.shell().info(colorize("  No matching log lines found.", :faint, color))
    else
      Mix.shell().info("")
      event_stream = if events != [], do: [{:event, events}], else: []
      merged = IssueStreams.merge(entries ++ event_stream)
      server_roles = Map.new(servers, fn {sid, meta} -> {sid, meta[:role]} end)

      Mix.shell().info(
        LogFormatting.format_merged(merged, color, log_opts.event_detail, server_roles)
      )
    end
  end

  defp prepare_log_data(issue, window, log_opts, servers) do
    filtered = IssueStreams.filter_servers(servers, log_opts.server_filter)
    filtered_map = Map.new(filtered)

    log_entries =
      LogAnalysis.extract(filtered_map, window,
        level_filter: log_opts.level_filter,
        excluded_ids: log_opts.excluded_ids
      )

    events =
      if log_opts.event_detail != :none,
        do: IssueStreams.extract_events(issue[:events] || [], window),
        else: []

    {log_entries, events}
  end

  defp extract_traffic_entries(issue, window, traffic_opts, server_ports) do
    traffic = issue[:traffic] || []

    if traffic == [] do
      []
    else
      server_ids =
        if traffic_opts.server_filter == :all do
          nil
        else
          issue[:servers]
          |> Kernel.||(%{})
          |> IssueStreams.filter_servers(traffic_opts.server_filter)
          |> Enum.map(&elem(&1, 0))
        end

      TrafficAnalysis.extract(traffic, server_ports, window,
        server_filter: server_ids,
        method_filter: traffic_opts.method_filter,
        endpoint_filter: traffic_opts.endpoint_filter,
        status_filter: traffic_opts.status_filter
      )
    end
  end

  defp print_traffic_entries(entries, server_ports, color) do
    output =
      Enum.map_join(entries, "\n", fn entry ->
        ts = format_traffic_timestamp(entry.timestamp, color)
        {server_id, _direction} = TrafficAnalysis.annotate_server(entry, server_ports)
        sid = server_id || "unknown"
        formatted = TrafficAnalysis.format_entry(entry)
        "#{ts} #{colorize(sid, :cyan, color)} #{formatted}"
      end)

    Mix.shell().info(output)
  end

  defp build_server_ports(deployments) do
    for {_dep_id, dep} <- deployments,
        {server_id, meta} <- dep.servers || %{},
        port = extract_port(meta[:endpoint]),
        port != nil,
        into: %{} do
      {port, server_id}
    end
  end

  defp extract_port(nil), do: nil

  defp extract_port(endpoint) do
    case URI.parse(endpoint) do
      %URI{port: port} when is_integer(port) -> port
      _ -> nil
    end
  end

  defp format_log_line(server_id, entry, server_roles, color) do
    role = server_roles[server_id]
    tag = LogFormatting.server_tag(server_id, role)
    line = LogFormatting.format_entry(entry, color)

    if color do
      num =
        server_id
        |> String.replace(~r/[^\d]/, "")
        |> then(&if(&1 == "", do: 0, else: String.to_integer(&1)))

      color_code = LogFormatting.server_color(role, num)

      [IO.ANSI.color(color_code), "[", tag, "] ", line, IO.ANSI.reset()]
      |> IO.iodata_to_binary()
    else
      "[#{tag}] #{line}"
    end
  end

  defp format_event_line(event, event_detail) do
    ts = event.timestamp |> DateTime.from_unix!(:microsecond) |> DateTime.to_iso8601()
    line = "  #{ts} #{LogFormatting.format_event(event)}"

    if event_detail == :full do
      line <> "\n    #{inspect(event, pretty: true, width: 120)}"
    else
      line
    end
  end

  defp format_traffic_line(entry, server_ports, color) do
    ts = format_traffic_timestamp(entry.timestamp, color)
    {server_id, _direction} = TrafficAnalysis.annotate_server(entry, server_ports)
    sid = server_id || "unknown"
    formatted = TrafficAnalysis.format_entry(entry)
    "#{ts} #{colorize(sid, :cyan, color)} #{formatted}"
  end

  defp format_traffic_timestamp(timestamp_us, color) when is_integer(timestamp_us) do
    secs = div(timestamp_us, 1_000_000)
    micros = rem(timestamp_us, 1_000_000)
    millis = div(micros, 1000)
    {:ok, dt} = DateTime.from_unix(secs)
    time_str = Calendar.strftime(dt, "%H:%M:%S") <> ".#{String.pad_leading("#{millis}", 3, "0")}"
    colorize(time_str, :faint, color)
  end
end
