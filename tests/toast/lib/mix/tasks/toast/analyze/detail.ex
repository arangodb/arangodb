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

defmodule Mix.Tasks.Toast.Analyze.Detail do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3, formatter_cb: 2]

  alias Mix.Tasks.Toast.Analyze.Data
  alias ToastTest.Enrichment
  alias ToastTest.Formatting.Issues
  alias Toast.Diagnostics.Coredump.ThreadFilter
  alias ToastTest.Formatting.Logs, as: LogFormatting
  alias ToastTest.LogAnalysis

  @issue_spec_keywords ~w(all crashes test_failures sanitizer timeouts infrastructure)

  def issue_spec?(arg) do
    arg in @issue_spec_keywords or Regex.match?(~r/^\d+(-\d+)?$/, arg)
  end

  def run(result_dir, opts, rest, color) do
    results = Data.load_results(result_dir)
    issues = Data.collect_issues(results, opts)
    indexed = Data.indexed_issues(issues)
    spec = parse_issue_spec(rest)
    selected = select_issues(indexed, spec)

    log_opts = %{
      enabled: logs_enabled?(opts),
      server_filter: LogAnalysis.parse_server_filter(opts[:log_servers]),
      window_spec: LogAnalysis.parse_window_spec(opts[:log_window]),
      event_detail: parse_event_detail(opts[:log_events]),
      level_filter: LogAnalysis.parse_level_filter(opts[:log_min_level]),
      excluded_ids: LogAnalysis.parse_exclude(opts[:log_exclude])
    }

    bt_opts = %{
      coredumps: Keyword.get(opts, :coredumps, true),
      threads: parse_threads_opt(opts[:threads]),
      max_frames: Keyword.get(opts, :backtrace_frames, 20),
      disassembly: Keyword.get(opts, :disassembly, false)
    }

    if selected == [] do
      Mix.shell().info(colorize("No matching issues.", :yellow, color))
    else
      Enum.each(selected, fn {issue, idx} ->
        print_issue_detail(issue, idx, color, log_opts, bt_opts)
      end)
    end
  end

  # --- Issue spec parsing ---

  defp parse_issue_spec([]), do: :all

  defp parse_issue_spec([keyword | _]) when keyword in @issue_spec_keywords,
    do: parse_keyword(keyword)

  defp parse_issue_spec([spec | _]) do
    unless Regex.match?(~r/^\d+(-\d+)?$/, spec) do
      Mix.raise(
        "Invalid issue spec: #{spec}. Use a number, range (2-4), or keyword (#{Enum.join(@issue_spec_keywords, ", ")})"
      )
    end

    case String.split(spec, "-", parts: 2) do
      [from, to] -> {:range, String.to_integer(from), String.to_integer(to)}
      [single] -> {:range, String.to_integer(single), String.to_integer(single)}
    end
  end

  defp parse_keyword("all"), do: :all
  defp parse_keyword("crashes"), do: {:type, :crash}
  defp parse_keyword("test_failures"), do: {:type, :test_failure}
  defp parse_keyword("sanitizer"), do: {:type, :sanitizer_report}
  defp parse_keyword("timeouts"), do: {:type, :timeout}
  defp parse_keyword("infrastructure"), do: {:type, :infrastructure}

  defp select_issues(indexed, :all), do: indexed

  defp select_issues(indexed, {:type, type}) do
    Enum.filter(indexed, fn {issue, _idx} -> issue.type == type end)
  end

  defp select_issues(indexed, {:range, from, to}) do
    Enum.filter(indexed, fn {_issue, idx} -> idx >= from and idx <= to end)
  end

  # --- Option parsing ---

  @valid_threads %{"relevant" => :relevant, "all" => :all}

  defp parse_threads_opt(nil), do: nil

  defp parse_threads_opt(value) do
    Map.get(@valid_threads, value) ||
      Mix.raise(
        "Unknown --threads value: #{value}. Valid: #{@valid_threads |> Map.keys() |> Enum.join(", ")}"
      )
  end

  @valid_event_details %{"none" => :none, "basic" => :basic, "full" => :full}

  defp parse_event_detail(nil), do: :basic

  defp parse_event_detail(level) do
    Map.get(@valid_event_details, level) ||
      Mix.raise(
        "Unknown --log-events level: #{level}. Valid: #{@valid_event_details |> Map.keys() |> Enum.join(", ")}"
      )
  end

  @log_implicit_enable_keys ~w(log_servers log_window log_min_level log_exclude)a

  defp logs_enabled?(opts) do
    opts[:logs] || Enum.any?(@log_implicit_enable_keys, &(opts[&1] != nil))
  end

  # --- Issue detail rendering ---

  defp print_issue_detail(issue, idx, color, log_opts, bt_opts) do
    bar = String.duplicate("─", 80)
    Mix.shell().info("\n#{colorize(bar, :faint, color)}")
    print_issue_header(issue, idx, color)
    print_issue_body(issue, color, bt_opts)
    if log_opts.enabled, do: print_issue_logs(issue, log_opts, color)
  end

  defp print_issue_header(issue, idx, color) do
    type_label = issue.type |> Atom.to_string() |> String.replace("_", " ") |> String.upcase()
    scope_label = Data.format_scope(issue)
    server_label = Data.format_server(issue)

    header =
      "#{colorize("##{idx}", :bright, color)} #{colorize(type_label, type_color(issue.type), color)}"

    Mix.shell().info(header)

    Mix.shell().info("  Scope:  #{scope_label}")

    if server_label != "\u2014" do
      Mix.shell().info("  Server: #{colorize(server_label, :cyan, color)}")
    end
  end

  defp type_color(:test_failure), do: :red
  defp type_color(:crash), do: :red
  defp type_color(:sanitizer_report), do: :yellow
  defp type_color(:timeout), do: :red
  defp type_color(:infrastructure), do: :red

  # --- Test failure detail ---

  defp print_issue_body(
         %{type: :test_failure, detail: %{test: %ExUnit.Test{state: {:failed, failures}} = test}},
         _color,
         _bt_opts
       ) do
    formatted =
      ExUnit.Formatter.format_test_failure(
        test,
        failures,
        1,
        :infinity,
        &formatter_cb/2
      )

    Mix.shell().info("\n#{formatted}")
  end

  # --- Sanitizer report detail ---

  defp print_issue_body(%{type: :sanitizer_report, detail: detail}, _color, _bt_opts) do
    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{Data.fmt_dt(detail.timestamp)}")
    end

    Mix.shell().info("")
    Mix.shell().info(detail.report)
  end

  # --- Crash detail ---

  defp print_issue_body(%{type: :crash, detail: detail}, color, bt_opts) do
    print_crash_info(detail, color)
    if bt_opts.coredumps, do: print_crash_backtrace(detail, color, bt_opts)
    print_crash_extra(detail, color, bt_opts)
  end

  # --- Timeout detail ---

  defp print_issue_body(%{type: :timeout, detail: detail}, color, _bt_opts) do
    label = Issues.timeout_source_label(detail.source)
    Mix.shell().info("  #{colorize("[#{label}] #{detail.reason}", :red, color)}")

    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{Data.fmt_dt(detail.timestamp)}")
    end

    if detail.servers != [] do
      Mix.shell().info("")
      Enum.each(detail.servers, &print_timeout_server(&1, color))
    end
  end

  # --- Infrastructure detail ---

  defp print_issue_body(%{type: :infrastructure, detail: detail} = issue, color, _bt_opts) do
    subtype = detail.subtype |> Atom.to_string() |> String.replace("_", " ")
    Mix.shell().info("  #{colorize(String.upcase(subtype), :red, color)}")

    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{Data.fmt_dt(detail.timestamp)}")
    end

    kind = if detail[:kind] == :deployment, do: "deployment", else: "system"

    Mix.shell().info(
      "  Total:  #{colorize("#{detail.total} #{kind} sockets", :red, color)} (threshold: #{detail.threshold})"
    )

    if detail[:kind] == :deployment do
      Mix.shell().info(
        "  Delta:  #{detail.deployment_delta} sockets since deployment (baseline: #{detail.baseline})"
      )
    end

    Mix.shell().info("")
    Mix.shell().info(colorize("  Per-server breakdown (by process ownership):", :bright, color))

    sorted = Enum.sort_by(detail.by_server, fn {_, v} -> -v.sockets.total end)

    server_total =
      Enum.reduce(sorted, 0, fn {server_id, server}, acc ->
        Mix.shell().info("    #{colorize(server_id, :cyan, color)}  #{server.sockets.total}")

        print_direction("← in ", server.sockets.in, color)
        print_direction("→ out", server.sockets.out, color)

        acc + server.sockets.total
      end)

    rest = detail.total - server_total

    if rest > 0 do
      Mix.shell().info("    #{colorize("(other)", :faint, color)}  #{rest}")
    end

    print_netstat_trajectory(issue, color)
  end

  defp print_direction(_label, stats, _color) when stats == %{}, do: :ok

  defp print_direction(label, stats, color) do
    formatted =
      stats
      |> Enum.sort_by(fn {_, n} -> -n end)
      |> Enum.map_join(", ", fn {state, n} -> "#{n} #{state}" end)

    Mix.shell().info("      #{colorize(label, :faint, color)}  #{formatted}")
  end

  @trajectory_limit 10

  defp print_netstat_trajectory(%{detail: %{timestamp: issue_ts}} = issue, color)
       when is_integer(issue_ts) do
    events = issue[:events] || []

    snapshots =
      events
      |> Enum.filter(&(&1.event == :netstat_snapshot))
      |> Enum.sort_by(& &1.timestamp)

    if snapshots != [] do
      entries = annotate_snapshots(snapshots, issue_ts, issue[:modules] || %{})

      print_top_contributors(entries, color)
      print_recent_trajectory(entries, length(snapshots), color)
    end
  end

  defp print_netstat_trajectory(_detail, _color), do: :ok

  @baseline_labels %{
    pre_deployment: "(pre-deployment)",
    deployment_ready: "(deployment ready)"
  }

  defp annotate_snapshots(snapshots, issue_ts, modules) do
    timeline = build_test_timeline(modules)

    {entries, _} =
      Enum.reduce(snapshots, {[], {0, timeline}}, fn snap, {acc, {prev, remaining}} ->
        delta = snap.total - prev
        marker = if snap.timestamp == issue_ts, do: :threshold, else: nil

        {label, remaining} =
          case Map.get(@baseline_labels, snap[:label]) do
            nil -> advance_timeline(snap.timestamp, remaining)
            baseline_label -> {baseline_label, remaining}
          end

        entry = %{
          total: snap.total,
          delta: delta,
          test: label,
          marker: marker,
          baseline: snap[:label] != nil
        }

        {[entry | acc], {snap.total, remaining}}
      end)

    Enum.reverse(entries)
  end

  defp print_top_contributors(entries, color) do
    top =
      entries
      |> Enum.filter(&(&1.delta > 0 and not &1.baseline))
      |> Enum.sort_by(& &1.delta, :desc)
      |> Enum.take(@trajectory_limit)

    if top != [] do
      Mix.shell().info("")
      Mix.shell().info(colorize("  Top contributors (by socket increase):", :bright, color))
      Enum.each(top, &print_trajectory_entry(&1, color))
    end
  end

  defp print_recent_trajectory(entries, total_count, color) do
    {baselines, test_entries} = Enum.split_while(entries, & &1.baseline)
    recent = Enum.take(test_entries, -@trajectory_limit)
    skipped = total_count - length(baselines) - length(recent)

    Mix.shell().info("")
    Mix.shell().info(colorize("  Recent socket count trajectory:", :bright, color))

    Enum.each(baselines, &print_trajectory_entry(&1, color))

    if skipped > 0 do
      Mix.shell().info(colorize("    ... #{skipped} earlier entries omitted", :faint, color))
    end

    Enum.each(recent, &print_trajectory_entry(&1, color))
  end

  defp print_trajectory_entry(entry, color) do
    delta_str = if entry.delta >= 0, do: "+#{entry.delta}", else: "#{entry.delta}"
    marker = if entry.marker == :threshold, do: " ← THRESHOLD", else: ""

    Mix.shell().info(
      "    #{colorize(String.pad_leading("#{entry.total}", 6), :bright, color)}  #{colorize(String.pad_leading(delta_str, 7), :faint, color)}  #{entry.test || "?"}#{colorize(marker, :red, color)}"
    )
  end

  defp build_test_timeline(modules) do
    for {mod, %{tests: tests}} <- modules,
        %{finished_at: %DateTime{} = finished_at} = test <- tests do
      {DateTime.to_unix(finished_at, :microsecond), "#{inspect(mod)}.#{test.name}"}
    end
    |> Enum.sort_by(&elem(&1, 0))
  end

  defp advance_timeline(snapshot_ts, timeline) do
    {passed, remaining} =
      Enum.split_while(timeline, fn {finished_us, _} -> finished_us <= snapshot_ts end)

    test_name = if passed == [], do: nil, else: passed |> List.last() |> elem(1)
    {test_name, remaining}
  end

  defp print_timeout_server(server, color) do
    pid_part = if server.os_pid, do: " (PID #{server.os_pid})", else: ""
    Mix.shell().info("  #{colorize("#{server.server_id}#{pid_part}", :cyan, color)}")

    if server.log_file do
      Mix.shell().info(colorize("    Log: #{server.log_file}", :blue, color))
    end

    if server[:coredump] do
      Mix.shell().info(colorize("    Coredump: #{server.coredump}", :blue, color))
    end
  end

  # --- Crash helpers ---

  defp print_crash_info(%{crash_info: info}, color) do
    parts =
      [
        if(info.os_pid, do: "PID #{info.os_pid}"),
        Issues.format_signal(info.signal),
        if(info.exit_status, do: "exit_status: #{info.exit_status}"),
        if(match?(%DateTime{}, info.timestamp), do: "at: #{DateTime.to_iso8601(info.timestamp)}")
      ]
      |> Toast.Utils.compact()

    if parts != [] do
      Mix.shell().info("  #{colorize(Enum.join(parts, "  "), :red, color)}")
    end
  end

  defp print_crash_info(_detail, _color), do: :ok

  # With --threads: show crash_lines (if any) then thread backtraces
  defp print_crash_backtrace(
         %{coredumps: [coredump | _]} = detail,
         color,
         %{threads: mode} = bt_opts
       )
       when mode in [:relevant, :all] do
    if is_binary(detail[:crash_lines]) do
      Mix.shell().info("")
      Mix.shell().info(detail.crash_lines)
    end

    threads = coredump.threads || []
    threads = if mode == :all, do: threads, else: ThreadFilter.filter_relevant(threads, coredump)

    if threads != [] do
      Mix.shell().info("")

      threads
      |> Enum.intersperse(:separator)
      |> Enum.each(fn
        :separator -> Mix.shell().info("")
        thread -> print_thread(thread, bt_opts.max_frames, color)
      end)
    end
  end

  # Default (no --threads): match post-exec summary output
  defp print_crash_backtrace(%{crash_lines: crash_lines}, _color, _bt_opts)
       when is_binary(crash_lines) do
    Mix.shell().info("")
    Mix.shell().info(crash_lines)
  end

  defp print_crash_backtrace(%{coredumps: [coredump | _]}, _color, _bt_opts) do
    case Issues.format_coredump_backtrace(coredump) do
      nil -> :ok
      text -> Mix.shell().info("\n#{text}")
    end
  end

  defp print_crash_backtrace(_detail, color, _bt_opts) do
    Mix.shell().info("  #{colorize("No crash details available.", :faint, color)}")
  end

  # Gated behind --disassembly since register dumps are large.
  defp print_crash_extra(%{coredumps: [coredump | _]}, color, bt_opts) do
    if bt_opts.disassembly do
      print_optional_section(Issues.format_registers(coredump), "Registers", color)
      print_optional_section(Issues.format_disassembly(coredump), "Disassembly", color)
    end
  end

  defp print_crash_extra(_detail, _color, _bt_opts), do: :ok

  defp print_optional_section(nil, _label, _color), do: :ok

  defp print_optional_section(text, label, color) do
    Mix.shell().info("")
    Mix.shell().info(colorize("#{label}:", :blue, color))
    Mix.shell().info(text)
  end

  defp print_thread(thread, max_frames, color) do
    os_part = if thread[:os_id], do: " (LWP #{thread.os_id})", else: ""
    header = "Thread #{thread.id}#{os_part}:"
    Mix.shell().info(colorize(header, :blue, color))

    frames = thread[:frames] || []
    shown = Enum.take(frames, max_frames)
    remaining = length(frames) - length(shown)

    backtrace = Enrichment.Coredump.format_backtrace(shown)
    Mix.shell().info(backtrace)
    if remaining > 0, do: Mix.shell().info("  ... (#{remaining} more frames)")
  end

  # --- Server logs ---

  defp print_issue_logs(issue, log_opts, color) do
    servers = issue[:servers] || %{}
    window = LogAnalysis.display_window(issue, log_opts.window_spec)
    bar = String.duplicate("─", 50)
    Mix.shell().info("\n#{colorize("── Server logs " <> bar, :faint, color)}")

    case window do
      nil ->
        Mix.shell().info(colorize("  No time bounds available for this issue.", :faint, color))

      {win_start, win_end} = window ->
        print_log_window(issue, window, win_start, win_end, log_opts, servers, color)
    end
  end

  defp print_log_window(issue, window, win_start, win_end, log_opts, servers, color) do
    print_log_context(issue, win_start, win_end, log_opts, servers, color)
    filtered = LogAnalysis.filter_servers(servers, log_opts.server_filter)
    filtered_map = Map.new(filtered)

    entries =
      LogAnalysis.extract(filtered_map, window,
        level_filter: log_opts.level_filter,
        excluded_ids: log_opts.excluded_ids
      )

    events =
      if log_opts.event_detail != :none,
        do: LogAnalysis.extract_events(issue[:events] || [], window),
        else: []

    if entries == [] and events == [] do
      Mix.shell().info(colorize("  No matching log lines found.", :faint, color))
    else
      Mix.shell().info("")
      merged = LogAnalysis.merge_streams(entries, events)
      server_roles = Map.new(servers, fn {sid, meta} -> {sid, meta[:role]} end)

      Mix.shell().info(
        LogFormatting.format_merged(merged, color, log_opts.event_detail, server_roles)
      )
    end
  end

  defp print_log_context(issue, win_start, win_end, log_opts, servers, color) do
    {tb_start, tb_end} = issue.time_bounds
    matching = LogAnalysis.matching_servers(servers, log_opts.server_filter)
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
        "  Log window:    #{Data.fmt_dt(win_start)} .. #{Data.fmt_dt(win_end)}",
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
