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

  alias Mix.Tasks.Toast.Analyze.Detail
  alias Mix.Tasks.Toast.Analyze.Info
  alias Mix.Tasks.Toast.Analyze.Issues
  alias Mix.Tasks.Toast.Analyze.Perf

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

  @subcommands %{
    "issues" => "issues",
    "detail" => "detail",
    "details" => "detail",
    "info" => "info",
    "perf" => "perf",
    "help" => "help"
  }

  @canonical_subcommands ~w(issues detail info perf help)

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
        "issues" -> Issues.run(result_dir, opts, color)
        "detail" -> Detail.run(result_dir, opts, rest, color)
        "info" -> Info.run(result_dir, opts, color)
        "perf" -> Perf.run(result_dir, opts, color)
      end
    end
  end

  defp pop_subcommand([cmd | rest]) when is_map_key(@subcommands, cmd),
    do: {@subcommands[cmd], rest}

  defp pop_subcommand([arg | _rest] = args) do
    if File.exists?(arg) or Detail.issue_spec?(arg) do
      {"issues", args}
    else
      message = "Unknown subcommand: #{arg}."
      suggestion = jaro_suggestion(@canonical_subcommands, arg)

      hint =
        if suggestion,
          do: " Did you mean `#{suggestion}`?",
          else: " Run `mix toast.analyze help` for usage."

      Mix.raise(message <> hint)
    end
  end

  defp pop_subcommand([]), do: {"issues", []}

  defp jaro_suggestion(candidates, input) do
    best = Enum.max_by(candidates, &String.jaro_distance(&1, input))
    if String.jaro_distance(best, input) >= 0.7, do: best
  end

  defp print_help do
    Mix.shell().info(@moduledoc)
  end

  defp pop_positional_result_dir(opts, [path | rest]) do
    if Detail.issue_spec?(path),
      do: {opts, [path | rest]},
      else: {Keyword.put_new(opts, :result_dir, path), rest}
  end

  defp pop_positional_result_dir(opts, []), do: {opts, []}
end
