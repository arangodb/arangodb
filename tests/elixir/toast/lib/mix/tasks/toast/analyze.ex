defmodule Mix.Tasks.Toast.Analyze do
  @shortdoc "Analyze Toast test results"
  @moduledoc """
  Analyzes Toast test results from `.diagnostics.etf` files.

  ## Usage

      mix toast.analyze [subcommand] [RESULT_DIR] [options]

  ## Subcommands

      issues          List all issues across all suites (default)
      detail[s]       Show full diagnostic detail for issues
      info            Show overview of diagnostics file contents
      perf            Performance analysis (module/test timing breakdown)

  ## Issue spec (detail only)

      mix toast.analyze detail 3           # single issue by index
      mix toast.analyze detail 2-4         # range of issues
      mix toast.analyze detail all         # all issues (default)
      mix toast.analyze detail crashes     # all crash issues
      mix toast.analyze detail sanitizer   # all sanitizer reports

  ## Options

      --result-dir <path>   Directory containing .diagnostics.etf files (default: ./toast-results)
      --no-color            Disable ANSI colors
      --type <type>         Filter by issue type: crash, test_failure, sanitizer_report, timeout
      --suite <name>        Filter to one suite

  ## Log options (detail only)

      --logs                          Enable server log display
      --log-servers <spec>            Server filter (default: all except agents)
      --log-window <before>,<after>   Signed milliseconds relative to issue time bounds (default: type-specific)
                                      Example: --log-window -20000,5000  (20s before, 5s after)
      --log-min-level <spec>          Filter log entries by level (default: show all)
                                      Examples: --log-min-level info
                                                --log-min-level info,crash=debug
      --log-exclude <ids>              Exclude log entries by ID (comma-separated)
      --log-events <level>            Event detail in log output: none, basic (default), full

  ## Backtrace options (detail only)

      --coredumps / --no-coredumps    Include coredump backtraces (default: on)
      --threads relevant|all          Show threads (relevant: likely interesting, all: every thread)
      --backtrace-frames N            Max frames per thread (default: 20)
      --disassembly / --no-disassembly  Show disassembly for hardware fault signals (default: off)

  ## Perf options

      --top N                         Limit entries shown per suite/module (default: 20)
      --module <prefix>               Drill into a specific module (prefix/substring match)
      --suite <name>                  Filter to one suite
  """

  use Mix.Task

  import ToastTest.Formatting, only: [colorize: 3, formatter_cb: 2]

  alias ToastTest.Enrichment
  alias ToastTest.IssueFormatting
  alias ToastTest.IssueFormatting.Logs

  @switches [
    result_dir: :string,
    color: :boolean,
    type: :string,
    suite: :string,
    logs: :boolean,
    log_servers: :string,
    log_window: :string,
    log_events: :string,
    log_exclude: :string,
    log_min_level: :string,
    coredumps: :boolean,
    threads: :string,
    backtrace_frames: :integer,
    disassembly: :boolean,
    top: :integer,
    module: :string,
    help: :boolean
  ]

  @valid_types %{
    "crash" => :crash,
    "test_failure" => :test_failure,
    "sanitizer_report" => :sanitizer_report,
    "timeout" => :timeout
  }

  @issue_spec_keywords ~w(all crashes test_failures sanitizer timeouts)

  @impl Mix.Task
  def run(args) do
    {opts, rest} = OptionParser.parse!(args, strict: @switches)

    if opts[:help] do
      print_help()
    else
      {subcommand, rest} = pop_subcommand(rest)
      {opts, rest} = pop_positional_result_dir(opts, rest)
      result_dir = Keyword.get(opts, :result_dir, Toast.Env.default_result_dir())
      color = Keyword.get(opts, :color, true)

      case subcommand do
        "help" -> print_help()
        "issues" -> run_issues(result_dir, opts, color)
        "detail" -> run_detail(result_dir, opts, rest, color)
        "info" -> run_info(result_dir, opts, color)
        "perf" -> run_perf(result_dir, opts, color)
      end
    end
  end

  @subcommands %{
    "issues" => "issues",
    "detail" => "detail",
    "details" => "detail",
    "info" => "info",
    "perf" => "perf",
    "help" => "help"
  }

  @canonical_subcommands ~w(issues detail info perf help)

  defp pop_subcommand([cmd | rest]) when is_map_key(@subcommands, cmd),
    do: {@subcommands[cmd], rest}

  defp pop_subcommand([arg | _rest] = args) do
    if File.exists?(arg) or issue_spec?(arg) do
      {"issues", args}
    else
      message = "Unknown subcommand: #{arg}."

      suggestion =
        @canonical_subcommands
        |> Enum.max_by(&String.jaro_distance(&1, arg))
        |> then(fn best ->
          if String.jaro_distance(best, arg) >= 0.7, do: best
        end)

      hint =
        if suggestion,
          do: " Did you mean `#{suggestion}`?",
          else: " Run `mix toast.analyze help` for usage."

      Mix.raise(message <> hint)
    end
  end

  defp pop_subcommand([]), do: {"issues", []}

  defp print_help do
    Mix.shell().info(@moduledoc)
  end

  defp pop_positional_result_dir(opts, [path | rest]) do
    if issue_spec?(path),
      do: {opts, [path | rest]},
      else: {Keyword.put_new(opts, :result_dir, path), rest}
  end

  defp pop_positional_result_dir(opts, []), do: {opts, []}

  defp issue_spec?(arg) do
    arg in @issue_spec_keywords or Regex.match?(~r/^\d+(-\d+)?$/, arg)
  end

  defp run_issues(result_dir, opts, color) do
    issues = result_dir |> load_results() |> collect_issues(opts)

    if issues == [] do
      Mix.shell().info(colorize("No issues found.", :green, color))
    else
      print_issues_table(issues, color)
      System.at_exit(fn _ -> exit({:shutdown, 1}) end)
    end
  end

  defp run_detail(result_dir, opts, rest, color) do
    results = load_results(result_dir)
    issues = collect_issues(results, opts)
    indexed = indexed_issues(issues)
    spec = parse_issue_spec(rest)
    selected = select_issues(indexed, spec)

    log_opts = %{
      enabled: logs_enabled?(opts),
      server_filter: Logs.parse_server_filter(opts[:log_servers]),
      window_spec: Logs.parse_window_spec(opts[:log_window]),
      event_detail: parse_event_detail(opts[:log_events]),
      level_filter: Logs.parse_level_filter(opts[:log_min_level]),
      excluded_ids: Logs.parse_exclude(opts[:log_exclude])
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

  defp run_info(result_dir, opts, color) do
    results = load_results(result_dir)

    results
    |> maybe_filter_suite(opts[:suite])
    |> Enum.each(&print_suite_info(&1, color))
  end

  defp maybe_filter_suite(results, nil), do: results

  defp maybe_filter_suite(results, suite) do
    Enum.filter(results, &(&1.suite == suite))
  end

  defp print_suite_info(result, color) do
    bar = String.duplicate("═", 80)
    Mix.shell().info("\n#{colorize(bar, :cyan, color)}")
    Mix.shell().info(colorize(" Suite: #{result.suite}", :bright, color))
    Mix.shell().info(colorize(bar, :cyan, color))

    # Time range
    started = if result.started_at, do: fmt_dt(result.started_at), else: "?"
    finished = if result.finished_at, do: fmt_dt(result.finished_at), else: "?"
    Mix.shell().info("  Time:    #{started} .. #{finished}")

    if result.times_us do
      run_s = Float.round((result.times_us[:run] || 0) / 1_000_000, 1)
      Mix.shell().info("  Runtime: #{run_s}s")
    end

    # Version
    Mix.shell().info("  Version: #{Map.get(result, :version, "?")}")

    # Modules & tests
    test_counts = count_tests(result.modules)

    Mix.shell().info(
      "  Modules: #{map_size(result.modules)}  Tests: #{test_counts.total} " <>
        "(#{test_counts.passed} passed, #{test_counts.failed} failed, #{test_counts.skipped} skipped)"
    )

    # Issues
    issue_counts =
      result.issues
      |> Enum.frequencies_by(& &1.type)

    Mix.shell().info("  Issues:  #{length(result.issues)} #{format_freq(issue_counts)}")

    # Events
    Mix.shell().info("  Events:  #{length(Map.get(result, :events, []))}")

    # Warnings
    warnings = Map.get(result, :warnings, [])

    if warnings != [] do
      Mix.shell().info("  Warnings: #{length(warnings)}")
    end

    # Deployments & servers
    deployments = Map.get(result, :deployments, %{})
    Mix.shell().info("")
    Mix.shell().info(colorize("  Deployments (#{map_size(deployments)}):", :bright, color))

    deployments
    |> Enum.sort_by(fn {did, _} -> did end)
    |> Enum.each(fn {did, deployment} ->
      mode = if deployment[:mode], do: " (#{deployment.mode})", else: ""
      Mix.shell().info("")
      Mix.shell().info("    #{colorize("#{did}#{mode}", :cyan, color)}")

      if deployment[:started_at] do
        stopped = if deployment[:stopped_at], do: fmt_dt(deployment.stopped_at), else: "running"
        Mix.shell().info("      Time: #{fmt_dt(deployment.started_at)} .. #{stopped}")
      end

      servers = Map.get(deployment, :servers, %{})

      servers
      |> Enum.sort_by(fn {sid, _} -> sid end)
      |> Enum.each(fn {sid, meta} ->
        print_server_info(sid, meta, color)
      end)
    end)

    # Coredump data
    print_coredump_data(Map.get(result, :coredumps, []), color)
  end

  defp print_coredump_data([], _color), do: :ok

  defp print_coredump_data(coredumps, color) do
    Mix.shell().info("")
    Mix.shell().info(colorize("  Coredumps (#{length(coredumps)}):", :bright, color))

    Enum.each(coredumps, fn cd ->
      threads = cd[:threads] || []
      thread_count = length(threads)
      debugger = if cd[:debugger], do: " (#{cd.debugger})", else: ""
      Mix.shell().info("")

      Mix.shell().info(
        "    #{colorize("#{cd.server_id}", :red, color)}  #{cd.core_path}#{debugger}"
      )

      summary =
        [
          if(cd[:signal], do: "signal: #{cd.signal}"),
          "#{thread_count} thread(s)",
          if(cd[:faulting_address], do: "fault addr: #{cd.faulting_address}"),
          if(cd[:crash_thread], do: "crash thread: #{cd.crash_thread}")
        ]
        |> Toast.Utils.compact()
        |> Enum.join(", ")

      Mix.shell().info("      #{summary}")
    end)
  end

  defp print_server_info(sid, meta, color) do
    role = if meta[:role], do: " (#{meta.role})", else: ""
    Mix.shell().info("      #{colorize("#{sid}#{role}", :cyan, color)}")

    parts = []

    parts =
      case meta[:endpoint] do
        ep when is_binary(ep) -> parts ++ ["endpoint=#{ep}"]
        _ -> parts
      end

    parts =
      case meta[:arango_id] do
        id when is_binary(id) -> parts ++ ["arango=#{id}"]
        _ -> parts
      end

    parts =
      case meta[:log_file] do
        f when is_binary(f) -> parts ++ ["log=#{f}"]
        _ -> parts
      end

    if parts != [] do
      Mix.shell().info("        #{Enum.join(parts, "  ")}")
    end

    # Incarnations
    incarnations = meta[:incarnations] || []

    if incarnations != [] do
      Enum.each(incarnations, fn inc ->
        stopped = if inc[:stopped_at], do: fmt_dt(inc.stopped_at), else: "running"

        Mix.shell().info("        pid=#{inc.pid}  #{fmt_dt(inc.started_at)} .. #{stopped}")
      end)
    end

    # Collected log windows
    logs = meta[:logs] || []
    total_entries = Enum.reduce(logs, 0, fn {_, _, entries}, acc -> acc + length(entries) end)

    Mix.shell().info("        Logs: #{length(logs)} window(s), #{total_entries} entries")
  end

  defp count_tests(modules) do
    Enum.reduce(modules, %{total: 0, passed: 0, failed: 0, skipped: 0}, fn {_mod, data}, acc ->
      Enum.reduce(data.tests, acc, fn test, acc ->
        acc = %{acc | total: acc.total + 1}

        case test.outcome do
          :passed -> %{acc | passed: acc.passed + 1}
          :failed -> %{acc | failed: acc.failed + 1}
          outcome when outcome in [:skipped, :excluded] -> %{acc | skipped: acc.skipped + 1}
          _ -> acc
        end
      end)
    end)
  end

  defp format_freq(freq) when map_size(freq) == 0, do: ""

  defp format_freq(freq) do
    parts = Enum.map_join(freq, ", ", fn {type, count} -> "#{count} #{type}" end)
    "(#{parts})"
  end

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
    scope_label = format_scope(issue.scope)
    server_label = format_server(issue)

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
    label = IssueFormatting.timeout_source_label(detail.source)
    Mix.shell().info("  #{colorize("[#{label}] #{detail.reason}", :red, color)}")

    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{DateTime.to_iso8601(detail.timestamp)}")
    end

    if detail.servers != [] do
      Mix.shell().info("")

      Enum.each(detail.servers, fn server ->
        pid_part = if server.os_pid, do: " (PID #{server.os_pid})", else: ""
        Mix.shell().info("  #{colorize("#{server.server_id}#{pid_part}", :cyan, color)}")

        if server.log_file do
          Mix.shell().info(colorize("    Log: #{server.log_file}", :blue, color))
        end

        if server[:coredump] do
          Mix.shell().info(colorize("    Coredump: #{server.coredump}", :blue, color))
        end
      end)
    end
  end

  # --- Crash helpers ---

  defp print_crash_info(%{crash_info: info}, color) do
    parts =
      [
        if(info.os_pid, do: "PID #{info.os_pid}"),
        IssueFormatting.format_signal(info.signal),
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
    case IssueFormatting.format_coredump_backtrace(coredump) do
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
      print_optional_section(IssueFormatting.format_registers(coredump), "Registers", color)
      print_optional_section(IssueFormatting.format_disassembly(coredump), "Disassembly", color)
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

  # Idle thread detection for --threads relevant filtering.
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

  # --- Server logs ---

  defp print_issue_logs(issue, log_opts, color) do
    servers = issue[:servers] || %{}
    window = Logs.display_window(issue, log_opts.window_spec)
    bar = String.duplicate("─", 50)
    Mix.shell().info("\n#{colorize("── Server logs " <> bar, :faint, color)}")

    case window do
      nil ->
        Mix.shell().info(colorize("  No time bounds available for this issue.", :faint, color))

      {win_start, win_end} = window ->
        print_log_context(issue, win_start, win_end, log_opts, servers, color)
        filtered = Logs.filter_servers(servers, log_opts.server_filter)
        filtered_map = Map.new(filtered)

        entries =
          Logs.extract(filtered_map, window,
            level_filter: log_opts.level_filter,
            excluded_ids: log_opts.excluded_ids
          )

        events =
          if log_opts.event_detail != :none,
            do: Logs.extract_events(issue[:events] || [], window),
            else: []

        if entries == [] and events == [] do
          Mix.shell().info(colorize("  No matching log lines found.", :faint, color))
        else
          Mix.shell().info("")
          merged = Logs.merge_streams(entries, events)
          server_roles = Map.new(servers, fn {sid, meta} -> {sid, meta[:role]} end)
          Mix.shell().info(Logs.format_merged(merged, color, log_opts.event_detail, server_roles))
        end
    end
  end

  defp print_log_context(issue, win_start, win_end, log_opts, servers, color) do
    {tb_start, tb_end} = issue.time_bounds
    matching = Logs.matching_servers(servers, log_opts.server_filter)
    deployments = issue[:deployments] || %{}

    Mix.shell().info(
      colorize("  Issue window:  #{fmt_dt(tb_start)} .. #{fmt_dt(tb_end)}", :faint, color)
    )

    Mix.shell().info(
      colorize("  Log window:    #{fmt_dt(win_start)} .. #{fmt_dt(win_end)}", :faint, color)
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
    tag = Logs.server_tag(server_id, meta[:role])

    parts = ["#{indent}#{tag}  #{server_id}"]

    parts =
      case meta do
        %{arango_id: id} when is_binary(id) -> parts ++ ["arango=#{id}"]
        _ -> parts
      end

    parts =
      case meta do
        %{endpoint: ep} when is_binary(ep) -> parts ++ ["endpoint=#{ep}"]
        _ -> parts
      end

    parts =
      case meta do
        %{incarnations: [%{pid: pid}]} ->
          parts ++ ["pid=#{pid}"]

        %{incarnations: incs} when length(incs) > 1 ->
          pids = Enum.map_join(incs, ",", &to_string(&1.pid))
          parts ++ ["pids=#{pids}"]

        _ ->
          parts
      end

    Enum.join(parts, "  ")
  end

  defp fmt_dt(%DateTime{} = dt), do: DateTime.to_iso8601(dt)
  defp fmt_dt(us) when is_integer(us), do: us |> DateTime.from_unix!(:microsecond) |> fmt_dt()

  # --- Shared infrastructure ---

  defp collect_issues(results, opts) do
    results
    |> Enum.flat_map(fn result ->
      all_servers = flatten_servers(result.deployments)
      deployments = Map.get(result, :deployments, %{})
      events = Map.get(result, :events, [])
      coredump_index = IssueFormatting.build_coredump_index(Map.get(result, :coredumps, []))

      result.issues
      |> Enum.map(&Map.put(&1, :suite, result.suite))
      |> Enum.map(&attach_time_bounds(&1, result.modules))
      |> Enum.map(&Map.put(&1, :servers, all_servers))
      |> Enum.map(&Map.put(&1, :deployments, deployments))
      |> Enum.map(&Map.put(&1, :events, events))
      |> then(&IssueFormatting.resolve_coredumps(&1, coredump_index))
    end)
    |> apply_filters(opts)
  end

  defp flatten_servers(deployments) do
    Enum.reduce(deployments, %{}, fn {_did, deployment}, acc ->
      Map.merge(acc, deployment.servers)
    end)
  end

  defp attach_time_bounds(
         %{type: :test_failure, scope: {:test, mod, name}} = issue,
         modules
       ) do
    case modules do
      %{^mod => %{tests: tests}} ->
        case Enum.find(tests, &(&1.name == name)) do
          %{started_at: %DateTime{} = s, finished_at: %DateTime{} = f} ->
            Map.put(
              issue,
              :time_bounds,
              {DateTime.to_unix(s, :microsecond), DateTime.to_unix(f, :microsecond)}
            )

          _ ->
            Map.put(issue, :time_bounds, nil)
        end

      _ ->
        Map.put(issue, :time_bounds, nil)
    end
  end

  defp attach_time_bounds(
         %{type: :crash, detail: %{crash_info: %{timestamp: ts}}} = issue,
         _modules
       )
       when is_integer(ts) do
    Map.put(issue, :time_bounds, {ts, ts})
  end

  defp attach_time_bounds(
         %{type: :sanitizer_report, detail: %{timestamp: ts}} = issue,
         _modules
       )
       when is_integer(ts) do
    Map.put(issue, :time_bounds, {ts, ts})
  end

  defp attach_time_bounds(
         %{type: :timeout, detail: %{timestamp: ts}} = issue,
         _modules
       )
       when is_integer(ts) do
    Map.put(issue, :time_bounds, {ts, ts})
  end

  defp attach_time_bounds(issue, _modules) do
    Map.put(issue, :time_bounds, nil)
  end

  defp indexed_issues(issues) do
    Enum.with_index(issues, 1)
  end

  defp load_results(result_dir) do
    pattern = Path.join(result_dir, "*.diagnostics.etf")

    case Path.wildcard(pattern) do
      [] ->
        Mix.raise("No .diagnostics.etf files found in #{result_dir}")

      files ->
        Enum.map(files, fn path ->
          path |> File.read!() |> :erlang.binary_to_term()
        end)
    end
  end

  defp apply_filters(issues, opts) do
    issues
    |> filter_by_type(opts[:type])
    |> filter_by_suite(opts[:suite])
  end

  defp filter_by_type(issues, nil), do: issues

  defp filter_by_type(issues, type_str) do
    type =
      Map.get(@valid_types, type_str) ||
        Mix.raise(
          "Unknown issue type: #{type_str}. Valid: #{@valid_types |> Map.keys() |> Enum.join(", ")}"
        )

    Enum.filter(issues, &(&1.type == type))
  end

  defp filter_by_suite(issues, nil), do: issues

  defp filter_by_suite(issues, suite) do
    Enum.filter(issues, &(&1.suite == suite))
  end

  # --- Perf subcommand ---

  @bar_width 30

  defp run_perf(result_dir, opts, color) do
    results =
      result_dir
      |> load_results()
      |> maybe_filter_suite(opts[:suite])

    top = Keyword.get(opts, :top, 20)

    case opts[:module] do
      nil -> print_suite_module_breakdown(results, top, color)
      prefix -> print_module_test_breakdown(results, prefix, top, color)
    end
  end

  defp print_suite_module_breakdown(results, top, color) do
    Mix.shell().info(
      "Legend: " <>
        colorize("██", :yellow, color) <>
        " setup  " <>
        colorize("██", :green, color) <>
        " tests  " <>
        colorize("██", :magenta, color) <>
        " teardown  " <>
        "░░ remaining"
    )

    Enum.each(results, fn result ->
      modules = result.modules
      if map_size(modules) == 0, do: :ok, else: print_suite_modules(result, modules, top, color)
    end)
  end

  defp print_suite_modules(result, modules, top, color) do
    test_count = modules |> Map.values() |> Enum.flat_map(& &1.tests) |> length()
    suite_duration_us = result.times_us[:run] || 0

    Mix.shell().info("")

    Mix.shell().info(
      colorize(
        "#{result.suite} (#{format_duration_us(suite_duration_us)} — #{test_count} tests)",
        :bright,
        color
      )
    )

    module_stats =
      modules
      |> Enum.map(fn {mod, data} -> module_timing(mod, data) end)
      |> Enum.sort_by(& &1.total_us, :desc)
      |> Enum.take(top)

    # Column widths
    mod_width =
      module_stats
      |> Enum.map(&String.length(format_module_name(&1.module)))
      |> Enum.max(fn -> 6 end)
      |> max(6)

    header =
      " " <>
        String.pad_trailing("Module", mod_width) <>
        "  Duration  Tests  Setup  Teardown"

    Mix.shell().info(colorize(header, :cyan, color))

    Enum.each(module_stats, fn stat ->
      pct = if suite_duration_us > 0, do: stat.total_us / suite_duration_us * 100, else: 0

      bar = phase_bar(stat, suite_duration_us, color)

      row =
        " " <>
          String.pad_trailing(format_module_name(stat.module), mod_width) <>
          "  " <>
          String.pad_leading(format_duration_us(stat.total_us), 8) <>
          "  " <>
          String.pad_leading(to_string(stat.test_count), 5) <>
          "  " <>
          String.pad_leading(format_duration_us(stat.setup_us), 5) <>
          "  " <>
          String.pad_leading(format_duration_us(stat.teardown_us), 8) <>
          "    " <>
          bar <>
          String.pad_leading("#{Float.round(pct, 0) |> trunc()}%", 5)

      Mix.shell().info(row)
    end)
  end

  defp print_module_test_breakdown(results, prefix, top, color) do
    matches =
      for result <- results,
          {mod, data} <- result.modules,
          mod_name = inspect(mod),
          match_module?(mod_name, prefix) do
        {result.suite, mod, data}
      end

    case matches do
      [] ->
        Mix.raise("No module matching \"#{prefix}\" found.")

      [{suite, mod, data}] ->
        print_test_breakdown(suite, mod, data, top, color)

      multiple ->
        names =
          Enum.map_join(multiple, "\n  ", fn {suite, mod, _} -> "#{inspect(mod)} (#{suite})" end)

        Mix.raise("Ambiguous module prefix \"#{prefix}\". Matches:\n  #{names}")
    end
  end

  defp match_module?(mod_name, prefix) do
    downcased = String.downcase(mod_name)
    pattern = String.downcase(prefix)
    String.starts_with?(downcased, pattern) or String.contains?(downcased, pattern)
  end

  defp print_test_breakdown(suite, mod, data, top, color) do
    stat = module_timing(mod, data)

    Mix.shell().info("")

    Mix.shell().info(
      colorize(
        "#{inspect(mod)} (#{suite}) — #{format_duration_us(stat.total_us)} total, " <>
          "setup #{format_duration_us(stat.setup_us)}, teardown #{format_duration_us(stat.teardown_us)}",
        :bright,
        color
      )
    )

    tests_us = stat.tests_us
    header = " " <> String.pad_trailing("Test", 40) <> "  Duration  Outcome"
    Mix.shell().info(colorize(header, :cyan, color))

    test_stats =
      data.tests
      |> Enum.sort_by(& &1.duration_us, :desc)
      |> Enum.take(top)

    Enum.each(test_stats, fn test ->
      name = ToastTest.Formatting.display_test_name(test.name)
      name = if String.length(name) > 40, do: String.slice(name, 0, 37) <> "...", else: name
      pct = if tests_us > 0, do: test.duration_us / tests_us * 100, else: 0

      bar = single_bar(test.duration_us, tests_us, :green, color)

      outcome_color =
        case test.outcome do
          :passed -> :green
          :failed -> :red
          _ -> :faint
        end

      row =
        " " <>
          String.pad_trailing(name, 40) <>
          "  " <>
          String.pad_leading(format_duration_us(test.duration_us), 8) <>
          "  " <>
          colorize(String.pad_trailing(Atom.to_string(test.outcome), 8), outcome_color, color) <>
          " " <>
          bar <>
          String.pad_leading("#{Float.round(pct, 0) |> trunc()}%", 5)

      Mix.shell().info(row)
    end)
  end

  defp module_timing(mod, data) do
    total_us = datetime_diff_us(data.started_at, data.finished_at)

    setup_us =
      if data.setup_finished_at,
        do: datetime_diff_us(data.started_at, data.setup_finished_at),
        else: 0

    teardown_us =
      if data.teardown_started_at,
        do: datetime_diff_us(data.teardown_started_at, data.finished_at),
        else: 0

    tests_us = max(total_us - setup_us - teardown_us, 0)

    %{
      module: mod,
      total_us: total_us,
      setup_us: setup_us,
      tests_us: tests_us,
      teardown_us: teardown_us,
      test_count: length(data.tests)
    }
  end

  defp datetime_diff_us(from, to) when not is_nil(from) and not is_nil(to) do
    DateTime.diff(to, from, :microsecond) |> max(0)
  end

  defp datetime_diff_us(_, _), do: 0

  defp phase_bar(stat, suite_total_us, color) when suite_total_us > 0 do
    total_cells = @bar_width
    fraction = stat.total_us / suite_total_us
    filled = round(fraction * total_cells) |> max(if(stat.total_us > 0, do: 1, else: 0))

    # Distribute filled cells among phases proportionally (largest-remainder method)
    phase_total = stat.setup_us + stat.tests_us + stat.teardown_us

    {setup_cells, test_cells, td_cells} =
      if phase_total > 0 do
        distribute_cells(
          [{stat.setup_us, :setup}, {stat.tests_us, :tests}, {stat.teardown_us, :td}],
          phase_total,
          filled
        )
      else
        {0, filled, 0}
      end

    empty = total_cells - setup_cells - test_cells - td_cells

    colorize(String.duplicate("█", setup_cells), :yellow, color) <>
      colorize(String.duplicate("█", test_cells), :green, color) <>
      colorize(String.duplicate("█", td_cells), :magenta, color) <>
      String.duplicate("░", empty)
  end

  defp phase_bar(_stat, _suite_total_us, _color) do
    String.duplicate("░", @bar_width)
  end

  # Largest-remainder method: floor each share, then distribute leftover cells
  # to phases with the largest fractional remainders.
  defp distribute_cells(phases, total_us, budget) do
    shares =
      Enum.map(phases, fn {us, label} ->
        exact = us / total_us * budget
        {label, trunc(exact), exact - trunc(exact)}
      end)

    floored = Enum.reduce(shares, 0, fn {_, f, _}, acc -> acc + f end)
    leftover = budget - floored

    # Award leftover cells to phases with largest fractional part
    awarded =
      shares
      |> Enum.sort_by(fn {_, _, frac} -> frac end, :desc)
      |> Enum.with_index()
      |> Enum.map(fn {{label, f, _frac}, i} ->
        {label, f + if(i < leftover, do: 1, else: 0)}
      end)

    result = Map.new(awarded)
    {Map.get(result, :setup, 0), Map.get(result, :tests, 0), Map.get(result, :td, 0)}
  end

  defp single_bar(value_us, total_us, bar_color, color) when total_us > 0 do
    filled = round(value_us / total_us * @bar_width) |> max(if(value_us > 0, do: 1, else: 0))
    empty = @bar_width - filled

    colorize(String.duplicate("█", filled), bar_color, color) <>
      String.duplicate("░", max(empty, 0))
  end

  defp single_bar(_, _, _, _), do: String.duplicate("░", @bar_width)

  defp format_duration_us(us) when us >= 60_000_000 do
    minutes = div(us, 60_000_000)
    secs = Float.round(rem(us, 60_000_000) / 1_000_000, 1)
    "#{minutes}m#{:erlang.float_to_binary(secs, decimals: 1)}s"
  end

  defp format_duration_us(us) when us >= 1_000_000 do
    secs = Float.round(us / 1_000_000, 1)
    "#{:erlang.float_to_binary(secs, decimals: 1)}s"
  end

  defp format_duration_us(us) when us >= 1_000 do
    ms = Float.round(us / 1_000, 0) |> trunc()
    "#{ms}ms"
  end

  defp format_duration_us(us), do: "#{us}µs"

  defp format_module_name(mod), do: inspect(mod)

  # --- Issues table ---

  defp print_issues_table(issues, color) do
    indexed = indexed_issues(issues)

    indexed
    |> Enum.group_by(fn {issue, _idx} -> issue.suite end)
    |> Enum.sort_by(fn {suite, _} -> suite end)
    |> Enum.each(fn {suite, suite_indexed} ->
      Mix.shell().info("")
      Mix.shell().info(colorize("#{suite} (#{length(suite_indexed)})", :bright, color))

      Mix.shell().info(
        colorize(String.duplicate("\u2500", String.length(suite) + 4), :faint, color)
      )

      rows =
        Enum.map(suite_indexed, fn {issue, idx} ->
          %{
            idx: to_string(idx),
            type: Atom.to_string(issue.type),
            scope: format_scope(issue.scope),
            server: format_server(issue)
          }
        end)

      widths = column_widths(rows)
      header = format_row(%{idx: "#", type: "Type", scope: "Scope", server: "Server"}, widths)
      Mix.shell().info(colorize(header, :cyan, color))
      Enum.each(rows, &Mix.shell().info(format_row(&1, widths)))
    end)
  end

  defp column_widths(rows) do
    headers = %{idx: "#", type: "Type", scope: "Scope", server: "Server"}
    all = [headers | rows]

    for key <- [:idx, :type, :scope, :server], into: %{} do
      width = all |> Enum.map(&String.length(Map.get(&1, key))) |> Enum.max()
      {key, width}
    end
  end

  defp format_row(row, widths) do
    [
      String.pad_leading(row.idx, widths.idx),
      "  ",
      String.pad_trailing(row.type, widths.type),
      "  ",
      String.pad_trailing(row.scope, widths.scope),
      "  ",
      row.server
    ]
    |> IO.iodata_to_binary()
  end

  # --- Formatting ---

  defp format_scope(scope), do: IssueFormatting.format_scope(scope) || ":suite"

  defp format_server(%{type: :crash, detail: %{server: server}}), do: server
  defp format_server(%{type: :sanitizer_report, detail: %{server: server}}), do: server

  defp format_server(%{type: :timeout, detail: %{servers: servers}}) when is_list(servers) do
    servers |> Enum.map(& &1.server_id) |> Enum.join(", ")
  end

  defp format_server(_), do: "\u2014"
end
