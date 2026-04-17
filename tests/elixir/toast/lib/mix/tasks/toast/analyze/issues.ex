defmodule Mix.Tasks.Toast.Analyze.Issues do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3]

  alias Mix.Tasks.Toast.Analyze.Data

  def run(result_dir, opts, color) do
    issues = result_dir |> Data.load_results() |> Data.collect_issues(opts)

    if issues == [] do
      Mix.shell().info(colorize("No issues found.", :green, color))
    else
      print_issues_table(issues, color)
      System.at_exit(fn _ -> exit({:shutdown, 1}) end)
    end
  end

  defp print_issues_table(issues, color) do
    indexed = Data.indexed_issues(issues)

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
            scope: Data.format_scope(issue),
            server: Data.format_server(issue)
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
end
