defmodule Mix.Tasks.Toast.Analyze do
  @shortdoc "Analyze Toast test results"
  @moduledoc """
  Analyzes Toast test results from `.diagnostics.etf` files.

  ## Usage

      mix toast.analyze issues [RESULT_DIR] [options]
      mix toast.analyze issues --result-dir <path> [options]

  ## Subcommands

      issues    List all issues across all suites (default)

  ## Options

      --result-dir <path>   Directory containing .diagnostics.etf files (default: ./results)
      --no-color            Disable ANSI colors
      --type <type>         Filter by issue type: crash, test_failure, sanitizer_report, timeout
      --suite <name>        Filter to one suite
  """

  use Mix.Task

  @preferred_cli_env :test

  @switches [
    result_dir: :string,
    color: :boolean,
    type: :string,
    suite: :string
  ]

  @impl Mix.Task
  def run(args) do
    {opts, rest} = OptionParser.parse!(args, strict: @switches)

    {subcommand, rest} = pop_subcommand(rest)
    opts = maybe_positional_result_dir(opts, rest)
    result_dir = Keyword.get(opts, :result_dir, "./results")
    color = Keyword.get(opts, :color, true)

    case subcommand do
      "issues" -> run_issues(result_dir, opts, color)
      other -> Mix.raise("Unknown subcommand: #{other}. Available: issues")
    end
  end

  defp pop_subcommand([cmd | rest]) when cmd in ~w(issues), do: {cmd, rest}
  defp pop_subcommand(rest), do: {"issues", rest}

  defp maybe_positional_result_dir(opts, [path | _]) do
    Keyword.put_new(opts, :result_dir, path)
  end

  defp maybe_positional_result_dir(opts, []), do: opts

  defp run_issues(result_dir, opts, color) do
    results = load_results(result_dir)

    issues =
      results
      |> Enum.flat_map(fn result ->
        Enum.map(result.issues, &Map.put(&1, :suite, result.suite))
      end)
      |> apply_filters(opts)

    if issues == [] do
      Mix.shell().info(maybe_color("No issues found.", :green, color))
    else
      print_issues_table(issues, color)
      System.at_exit(fn _ -> exit({:shutdown, 1}) end)
    end
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
    type = String.to_existing_atom(type_str)
    Enum.filter(issues, &(&1.type == type))
  end

  defp filter_by_suite(issues, nil), do: issues

  defp filter_by_suite(issues, suite) do
    Enum.filter(issues, &(&1.suite == suite))
  end

  defp print_issues_table(issues, color) do
    issues
    |> Enum.group_by(& &1.suite)
    |> Enum.sort_by(fn {suite, _} -> suite end)
    |> Enum.reduce(1, fn {suite, suite_issues}, global_idx ->
      Mix.shell().info("")
      Mix.shell().info(maybe_color("#{suite} (#{length(suite_issues)})", :bright, color))

      Mix.shell().info(
        maybe_color(String.duplicate("\u2500", String.length(suite) + 4), :faint, color)
      )

      rows =
        suite_issues
        |> Enum.with_index(global_idx)
        |> Enum.map(fn {issue, idx} ->
          %{
            idx: to_string(idx),
            type: format_type(issue.type),
            scope: format_scope(issue.scope),
            server: format_server(issue)
          }
        end)

      widths = column_widths(rows)
      header = format_row(%{idx: "#", type: "Type", scope: "Scope", server: "Server"}, widths)
      Mix.shell().info(maybe_color(header, :cyan, color))
      Enum.each(rows, &Mix.shell().info(format_row(&1, widths)))

      global_idx + length(suite_issues)
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

  defp format_type(type), do: Atom.to_string(type)

  defp format_scope(:suite), do: ":suite"

  defp format_scope({:module, mod}) do
    inspect(mod)
  end

  defp format_scope({:test, mod, test_name}) do
    name =
      test_name
      |> Atom.to_string()
      |> String.replace_prefix("test ", "")

    "#{inspect(mod)} > \"#{name}\""
  end

  defp format_server(%{type: :crash, detail: %{server: server}}), do: server
  defp format_server(%{type: :sanitizer_report, detail: %{server: server}}), do: server

  defp format_server(%{type: :timeout, detail: %{servers: servers}}) when is_list(servers) do
    servers |> Enum.map(& &1.server_id) |> Enum.join(", ")
  end

  defp format_server(_), do: "\u2014"

  defp maybe_color(text, _color, false), do: text
  defp maybe_color(text, color, true), do: IO.ANSI.format([color, text]) |> IO.iodata_to_binary()
end
