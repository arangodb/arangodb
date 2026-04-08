defmodule Mix.Tasks.Toast.Analyze.Data do
  @moduledoc false

  alias ToastTest.Formatting.Issues

  @valid_types %{
    "crash" => :crash,
    "test_failure" => :test_failure,
    "sanitizer_report" => :sanitizer_report,
    "timeout" => :timeout
  }

  def load_results(result_dir) do
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

  def collect_issues(results, opts) do
    results
    |> Enum.flat_map(fn result ->
      all_servers = flatten_servers(result.deployments)
      deployments = Map.get(result, :deployments, %{})
      events = Map.get(result, :events, [])
      coredump_index = Issues.build_coredump_index(Map.get(result, :coredumps, []))

      result.issues
      |> Enum.map(&Map.put(&1, :suite, result.suite))
      |> Enum.map(&attach_time_bounds(&1, result.modules))
      |> Enum.map(&Map.put(&1, :servers, all_servers))
      |> Enum.map(&Map.put(&1, :deployments, deployments))
      |> Enum.map(&Map.put(&1, :events, events))
      |> then(&Issues.resolve_coredumps(&1, coredump_index))
    end)
    |> apply_filters(opts)
  end

  def indexed_issues(issues) do
    Enum.with_index(issues, 1)
  end

  def maybe_filter_suite(results, nil), do: results

  def maybe_filter_suite(results, suite) do
    Enum.filter(results, &(&1.suite == suite))
  end

  # --- Formatting helpers used by multiple subcommands ---

  def fmt_dt(%DateTime{} = dt), do: DateTime.to_iso8601(dt)
  def fmt_dt(us) when is_integer(us), do: us |> DateTime.from_unix!(:microsecond) |> fmt_dt()

  def format_scope(scope), do: Issues.format_scope(scope) || ":suite"

  def format_server(%{type: :crash, detail: %{server: server}}), do: server
  def format_server(%{type: :sanitizer_report, detail: %{server: server}}), do: server

  def format_server(%{type: :timeout, detail: %{servers: servers}}) when is_list(servers) do
    servers |> Enum.map_join(", ", & &1.server_id)
  end

  def format_server(_), do: "\u2014"

  # --- Private ---

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
end
