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
  """

  use Mix.Task

  import ToastTest.Formatting, only: [colorize: 3, formatter_cb: 2]

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
      result_dir = Keyword.get(opts, :result_dir, "./toast-results")
      color = Keyword.get(opts, :color, true)

      case subcommand do
        "help" -> print_help()
        "issues" -> run_issues(result_dir, opts, color)
        "detail" -> run_detail(result_dir, opts, rest, color)
        "info" -> run_info(result_dir, opts, color)
      end
    end
  end

  @subcommands %{
    "issues" => "issues",
    "detail" => "detail",
    "details" => "detail",
    "info" => "info",
    "help" => "help"
  }

  @canonical_subcommands ~w(issues detail info help)

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

    if selected == [] do
      Mix.shell().info(colorize("No matching issues.", :yellow, color))
    else
      Enum.each(selected, fn {issue, idx} ->
        print_issue_detail(issue, idx, color, log_opts)
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

  defp print_issue_detail(issue, idx, color, log_opts) do
    bar = String.duplicate("─", 80)
    Mix.shell().info("\n#{colorize(bar, :faint, color)}")
    print_issue_header(issue, idx, color)
    print_issue_body(issue, color)
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
         _color
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

  defp print_issue_body(%{type: :sanitizer_report, detail: detail}, _color) do
    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{DateTime.to_iso8601(detail.timestamp)}")
    end

    Mix.shell().info("")
    Mix.shell().info(detail.report)
  end

  # --- Crash detail ---

  defp print_issue_body(%{type: :crash, detail: detail}, color) do
    print_crash_info(detail, color)
    print_crash_backtrace(detail, color)
  end

  # --- Timeout detail ---

  defp print_issue_body(%{type: :timeout, detail: detail}, color) do
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
      |> Enum.reject(&is_nil/1)

    if parts != [] do
      Mix.shell().info("  #{colorize(Enum.join(parts, "  "), :red, color)}")
    end
  end

  defp print_crash_info(_detail, _color), do: :ok

  defp format_signal(nil), do: nil

  defp format_signal(sig) do
    case :exec.signal(sig) do
      name when is_atom(name) ->
        "signal: #{name |> Atom.to_string() |> String.upcase()} (#{sig})"

      _ ->
        "signal: #{sig}"
    end
  end

  defp print_crash_backtrace(%{coredumps: [coredump | _]}, _color) do
    case coredump.threads do
      [thread | _] ->
        Mix.shell().info("")
        Mix.shell().info(thread.backtrace)

      _ ->
        :ok
    end
  end

  defp print_crash_backtrace(%{crash_lines: crash_lines}, _color) when is_binary(crash_lines) do
    Mix.shell().info("")
    Mix.shell().info(crash_lines)
  end

  defp print_crash_backtrace(_detail, color) do
    Mix.shell().info("  #{colorize("No crash details available.", :faint, color)}")
  end

  defp timeout_source_label(:startup_timeout), do: "Startup Timeout"
  defp timeout_source_label(:shutdown_timeout), do: "Shutdown Timeout"
  defp timeout_source_label(:test_timeout), do: "Test Timeout"
  defp timeout_source_label(:global_timeout), do: "Global Timeout"
  defp timeout_source_label(other), do: "Timeout: #{other}"

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

      result.issues
      |> Enum.map(&Map.put(&1, :suite, result.suite))
      |> Enum.map(&attach_time_bounds(&1, result.modules))
      |> Enum.map(&Map.put(&1, :servers, all_servers))
      |> Enum.map(&Map.put(&1, :deployments, deployments))
      |> Enum.map(&Map.put(&1, :events, events))
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

  defp format_scope(:suite), do: ":suite"
  defp format_scope({:module, mod}), do: inspect(mod)

  defp format_scope({:test, mod, test_name}) do
    name = test_name |> Atom.to_string() |> String.replace_prefix("test ", "")
    "#{inspect(mod)} > \"#{name}\""
  end

  defp format_server(%{type: :crash, detail: %{server: server}}), do: server
  defp format_server(%{type: :sanitizer_report, detail: %{server: server}}), do: server

  defp format_server(%{type: :timeout, detail: %{servers: servers}}) when is_list(servers) do
    servers |> Enum.map(& &1.server_id) |> Enum.join(", ")
  end

  defp format_server(_), do: "\u2014"
end
