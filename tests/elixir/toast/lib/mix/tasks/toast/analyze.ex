defmodule Mix.Tasks.Toast.Analyze do
  @shortdoc "Analyze Toast test results"
  @moduledoc """
  Analyzes Toast test results from `.diagnostics.etf` files.

  ## Usage

      mix toast.analyze [subcommand] [RESULT_DIR] [options]

  ## Subcommands

      issues          List all issues across all suites (default)
      detail[s]       Show full diagnostic detail for issues

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
  """

  use Mix.Task

  import ToastTest.Formatting, only: [colorize: 3, formatter_cb: 2]

  @switches [
    result_dir: :string,
    color: :boolean,
    type: :string,
    suite: :string,
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
      end
    end
  end

  @subcommands %{
    "issues" => "issues",
    "detail" => "detail",
    "details" => "detail",
    "help" => "help"
  }

  @canonical_subcommands ~w(issues detail help)

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

    if selected == [] do
      Mix.shell().info(colorize("No matching issues.", :yellow, color))
    else
      Enum.each(selected, fn {issue, idx} ->
        print_issue_detail(issue, idx, color)
      end)
    end
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

  defp print_issue_detail(issue, idx, color) do
    bar = String.duplicate("\u2500", 80)
    Mix.shell().info("\n#{colorize(bar, :faint, color)}")
    print_issue_header(issue, idx, color)
    print_issue_body(issue, color)
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
    label = timeout_source_label(detail.source)
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
        format_signal(info.signal),
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

  # --- Shared infrastructure ---

  defp collect_issues(results, opts) do
    results
    |> Enum.flat_map(fn result ->
      Enum.map(result.issues, &Map.put(&1, :suite, result.suite))
    end)
    |> apply_filters(opts)
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
