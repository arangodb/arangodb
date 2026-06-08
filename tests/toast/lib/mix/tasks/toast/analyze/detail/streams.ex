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

  alias Mix.Tasks.Toast.Analyze.Data
  alias Mix.Tasks.Toast.Analyze.Detail.ServerInfo
  alias ToastTest.Formatting.Logs, as: LogFormatting
  alias ToastTest.Analyze.IssueStreams
  alias ToastTest.Analyze.Logs, as: LogAnalysis
  alias ToastTest.Traffic.Analysis, as: TrafficAnalysis

  def print(issue, display, color) do
    label =
      stream_label(display.log.enabled, display.traffic.enabled, display.event.detail != :none)

    bar = String.duplicate("─", 50)
    Mix.shell().info("\n#{colorize("── #{label} " <> bar, :faint, color)}")

    case resolve_window(issue, display.log, display.traffic) do
      nil ->
        Mix.shell().info(colorize("  No time bounds available for this issue.", :faint, color))

      window ->
        servers = issue[:servers] || %{}
        server_ports = build_server_ports(servers)

        server_filter =
          merge_server_filters(
            if(display.log.enabled, do: display.log.server_filter),
            if(display.traffic.enabled, do: display.traffic.server_filter)
          )

        ServerInfo.print_context(issue, window, server_filter, servers, color)

        streams = build_streams(issue, window, display, servers, server_ports)

        render_ctx = %{
          event_detail: display.event.detail,
          server_ports: server_ports,
          server_roles: Map.new(servers, fn {sid, meta} -> {sid, meta[:role]} end),
          format_opts: %{
            limit: display.traffic.body_limit,
            raw: display.traffic.raw_body,
            all_headers: display.traffic.all_headers
          },
          color: color
        }

        print_entries(streams, render_ctx)
    end
  end

  # --- Stream assembly ---

  defp build_streams(issue, window, display, servers, server_ports) do
    log_entries = extract_log_entries(window, display.log, servers)
    traffic_entries = extract_traffic_entries(issue, window, display.traffic, server_ports)
    events = extract_events(issue, window, display.event)

    colored_traffic = TrafficAnalysis.assign_pair_colors(traffic_entries)

    event_stream = if events != [], do: [{:event, events}], else: []
    traffic_stream = if colored_traffic != [], do: [{:traffic, colored_traffic}], else: []
    log_entries ++ event_stream ++ traffic_stream
  end

  defp extract_log_entries(_window, %{enabled: false}, _servers), do: []

  defp extract_log_entries(window, log_opts, servers) do
    filtered = IssueStreams.filter_servers(servers, log_opts.server_filter)
    filtered_map = Map.new(filtered)

    LogAnalysis.extract(filtered_map, window,
      level_filter: log_opts.level_filter,
      excluded_ids: log_opts.excluded_ids
    )
  end

  defp extract_events(issue, window, event_opts) do
    if event_opts.detail != :none,
      do: IssueStreams.extract_events(issue[:events] || [], window),
      else: []
  end

  defp extract_traffic_entries(%{traffic: traffic} = iss, window, %{enabled: true} = opts, ports)
       when traffic != [] do
    do_extract_traffic(traffic, iss, window, opts, ports)
  end

  defp extract_traffic_entries(_issue, _window, _traffic_opts, _ports), do: []

  defp do_extract_traffic(traffic, issue, window, traffic_opts, server_ports) do
    server_ids = resolve_traffic_server_ids(traffic_opts.server_filter, issue)

    TrafficAnalysis.extract(traffic, server_ports, window,
      server_filter: server_ids,
      method_filter: traffic_opts.method_filter,
      endpoint_filter: traffic_opts.endpoint_filter,
      status_filter: traffic_opts.status_filter
    )
  end

  defp resolve_traffic_server_ids(:all, _issue), do: nil

  defp resolve_traffic_server_ids(filter, issue) do
    (issue[:servers] || %{})
    |> IssueStreams.filter_servers(filter)
    |> Enum.map(&elem(&1, 0))
  end

  # --- Rendering ---

  defp print_entries([], render_ctx),
    do: Mix.shell().info(colorize("  No matching entries.", :faint, render_ctx.color))

  defp print_entries(streams, render_ctx) do
    Mix.shell().info("")

    streams
    |> IssueStreams.merge()
    |> Enum.each(fn
      {:event, event} -> format_event_line(event, render_ctx) |> Mix.shell().info()
      {:traffic, entry} -> format_traffic_line(entry, render_ctx) |> Mix.shell().info()
      {server_id, entry} -> format_log_line(server_id, entry, render_ctx) |> Mix.shell().info()
    end)
  end

  defp format_log_line(server_id, entry, render_ctx) do
    role = render_ctx.server_roles[server_id]
    line = LogFormatting.format_entry(entry, render_ctx.color)
    LogFormatting.format_tagged_line(server_id, role, line, render_ctx.color)
  end

  defp format_event_line(event, render_ctx) do
    ts = colorize(Data.fmt_dt(event.timestamp), :faint, render_ctx.color)
    line = "  #{ts} #{LogFormatting.format_event(event)}"

    if render_ctx.event_detail == :full do
      line <> "\n    #{inspect(event, pretty: true, width: 120)}"
    else
      line
    end
  end

  @detail_color 243

  defp format_traffic_line(entry, render_ctx) do
    {server_id, _direction} = TrafficAnalysis.annotate_server(entry, render_ctx.server_ports)
    sid = server_id || "unknown"
    role = render_ctx.server_roles[sid]
    {summary, detail} = TrafficAnalysis.format_entry_parts(entry, render_ctx.format_opts)

    ts = colorize(Data.fmt_dt(entry.timestamp), :faint, render_ctx.color)
    server_tag = LogFormatting.format_tagged_line(sid, role, "", render_ctx.color)

    summary_line =
      if render_ctx.color && entry[:pair_color] do
        [ts, " ", server_tag, IO.ANSI.color(entry.pair_color), summary, IO.ANSI.reset()]
      else
        [ts, " ", server_tag, summary]
      end

    if detail do
      detail_line =
        if render_ctx.color do
          [IO.ANSI.color(@detail_color), detail, IO.ANSI.reset()]
        else
          detail
        end

      [summary_line, "\n", detail_line]
    else
      summary_line
    end
  end

  # --- Window resolution ---

  defp resolve_window(issue, log_opts, traffic_opts) do
    windows =
      []
      |> maybe_add_window(log_opts.enabled, issue, log_opts.window_spec)
      |> maybe_add_window(traffic_opts.enabled, issue, traffic_opts.window_spec)

    case windows do
      # Events-only: fall back to issue-type default window
      [] -> IssueStreams.display_window(issue, nil)
      [single] -> single
      multiple -> union_windows(multiple)
    end
  end

  defp maybe_add_window(acc, false, _issue, _spec), do: acc

  defp maybe_add_window(acc, true, issue, spec) do
    case IssueStreams.display_window(issue, spec) do
      nil -> acc
      window -> [window | acc]
    end
  end

  defp union_windows(windows) do
    {starts, ends} = Enum.unzip(windows)
    {Enum.min(starts), Enum.max(ends)}
  end

  # --- Helpers ---

  defp stream_label(logs?, traffic?, events?) do
    labels =
      [{logs?, "Logs"}, {traffic?, "Traffic"}, {events?, "Events"}]
      |> Enum.filter(&elem(&1, 0))
      |> Enum.map(&elem(&1, 1))

    case labels do
      [] -> "Streams"
      ["Logs"] -> "Server logs"
      ["Logs", "Events"] -> "Server logs + Events"
      _ -> Enum.join(labels, " + ")
    end
  end

  defp merge_server_filters(nil, nil), do: :all
  defp merge_server_filters(:all, _), do: :all
  defp merge_server_filters(_, :all), do: :all
  defp merge_server_filters(a, nil), do: a
  defp merge_server_filters(nil, b), do: b
  defp merge_server_filters(a, b), do: Enum.uniq(a ++ b)

  defp build_server_ports(servers) do
    for {server_id, meta} <- servers,
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
end
