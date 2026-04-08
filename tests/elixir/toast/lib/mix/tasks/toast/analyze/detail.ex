defmodule Mix.Tasks.Toast.Analyze.Detail do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3, formatter_cb: 2]

  alias Mix.Tasks.Toast.Analyze.Data
  alias ToastTest.Enrichment
  alias ToastTest.Formatting.Issues
  alias ToastTest.Formatting.Logs, as: LogFormatting
  alias ToastTest.LogAnalysis

  @issue_spec_keywords ~w(all crashes test_failures sanitizer timeouts)

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
    scope_label = Data.format_scope(issue.scope)
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
      Mix.shell().info("  Time:   #{DateTime.to_iso8601(detail.timestamp)}")
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
      Mix.shell().info("  Time:   #{DateTime.to_iso8601(detail.timestamp)}")
    end

    if detail.servers != [] do
      Mix.shell().info("")
      Enum.each(detail.servers, &print_timeout_server(&1, color))
    end
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
    threads = if mode == :all, do: threads, else: filter_relevant_threads(threads, coredump)

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

  # --- Idle thread filtering for --threads relevant ---
  #
  # A thread is considered idle (irrelevant) if it's waiting for work
  # rather than actively doing something.  The crash thread is always kept.
  #
  # Two categories:
  # 1. Immediate idle — the function IS the blocking call (epoll, asio event wait).
  #    Presence anywhere in the backtrace → thread is idle.
  # 2. Conditional idle — thread pool / worker loops that are only idle when the
  #    frame directly above them (closer to top of stack) is a condition wait.

  @immediate_idle_functions [
    "epoll_wait",
    "boost::asio::detail::posix_event::wait"
  ]

  @cond_wait_patterns [
    "pthread_cond_wait",
    "pthread_cond_timedwait",
    "pthread_cond_clockwait",
    "std::condition_variable::wait_for"
  ]

  @idle_loop_functions [
    # gdb unnamed symbol
    "??",
    "___lldb_unnamed_symbol",
    "arangodb::application_features::ApplicationServer::wait",
    "arangodb::async_registry::Feature::PromiseCleanupThread",
    "arangodb::CacheRebalancerThread::run",
    "arangodb::IOHeartbeatThread::run",
    "arangodb::RocksDBBackgroundThread::run",
    "arangodb::RocksDBIndexCacheRefillThread::run",
    "arangodb::RocksDBSyncThread::run",
    "arangodb::Scheduler::runCronThread",
    "arangodb::StatisticsWorker::run",
    "arangodb::SupervisedScheduler::getWork",
    "arangodb::SupervisedScheduler::runSupervisor",
    "arangodb::TtlThread::run",
    "arangodb::V8DealerFeature::collectGarbage",
    "background_thread_sleep",
    "background_work_sleep_once",
    "boost::asio::detail::scheduler::do_run_one",
    "irs::async_utils::ThreadPool",
    "rocksdb::ThreadPoolImpl::Impl::BGThread"
  ]

  @idle_loop_files [
    "default-worker-threads-task-runner"
  ]

  defp filter_relevant_threads(threads, coredump) do
    crash_id = coredump[:crash_thread]

    kept =
      Enum.reject(threads, fn thread ->
        to_string(thread.id) != to_string(crash_id) and idle_thread?(thread[:frames] || [])
      end)

    # If nothing survived (e.g., no crash_thread set), keep at least the first thread.
    if kept == [], do: Enum.take(threads, 1), else: kept
  end

  defp idle_thread?(frames) do
    frames == [] or immediate_idle?(frames) or cond_wait_idle?(frames)
  end

  defp immediate_idle?(frames) do
    # Use String.contains? because LLDB may inline functions into a single frame,
    # e.g., "do_run_one(...) [inlined] posix_event::wait(...)".
    Enum.any?(frames, fn frame ->
      Enum.any?(@immediate_idle_functions, &String.contains?(frame.function, &1))
    end)
  end

  defp cond_wait_idle?(frames) do
    # Check consecutive frame pairs [inner (closer to top), outer (closer to bottom)].
    # If inner is a cond_wait and outer is an idle loop → thread is idle.
    # Use String.contains? because LLDB includes argument lists in function names
    # and may inline multiple functions into a single frame.
    frames
    |> Enum.chunk_every(2, 1, :discard)
    |> Enum.any?(fn [inner, outer] ->
      cond_wait_frame?(inner) and idle_loop_frame?(outer)
    end)
  end

  defp cond_wait_frame?(frame) do
    Enum.any?(@cond_wait_patterns, &String.contains?(frame.function, &1))
  end

  defp idle_loop_frame?(frame) do
    Enum.any?(@idle_loop_functions, &String.contains?(frame.function, &1)) or
      (frame[:file] != nil and
         Enum.any?(@idle_loop_files, &String.contains?(frame.file, &1)))
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
