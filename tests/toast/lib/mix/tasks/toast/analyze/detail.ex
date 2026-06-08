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

  import ToastTest.Formatting, only: [colorize: 3]

  alias Mix.Tasks.Toast.Analyze.Data
  alias Mix.Tasks.Toast.Analyze.Detail.Body
  alias Mix.Tasks.Toast.Analyze.Detail.Streams
  alias ToastTest.Analyze.IssueStreams
  alias ToastTest.Analyze.Logs, as: LogAnalysis
  @issue_spec_keywords ~w(all crashes test_failures sanitizer timeouts infrastructure)

  def issue_spec?(arg) do
    arg in @issue_spec_keywords or Regex.match?(~r/^\d+(-\d+)?$/, arg)
  end

  def run(result_dir, opts, rest, color) do
    results = Data.load_results(result_dir)
    issues = Data.collect_issues(results, opts)
    indexed = Enum.with_index(issues, 1)
    spec = parse_issue_spec(rest)
    selected = select_issues(indexed, spec)

    logs_enabled = logs_enabled?(opts)
    traffic_enabled = traffic_enabled?(opts)

    log_opts = %{
      enabled: logs_enabled,
      server_filter: IssueStreams.parse_server_filter(opts[:log_servers]),
      window_spec: IssueStreams.parse_window_spec(opts[:log_window]),
      level_filter: LogAnalysis.parse_level_filter(opts[:log_min_level]),
      excluded_ids: LogAnalysis.parse_exclude(opts[:log_exclude])
    }

    traffic_opts = %{
      enabled: traffic_enabled,
      server_filter: IssueStreams.parse_server_filter(opts[:traffic_servers]),
      window_spec: IssueStreams.parse_window_spec(opts[:traffic_window]),
      method_filter: parse_list_filter(opts[:traffic_methods]),
      endpoint_filter: parse_list_filter(opts[:traffic_endpoints]),
      status_filter: parse_status_filter(opts[:traffic_status]),
      body_limit: parse_body_limit(opts[:traffic_body_limit]),
      raw_body: Keyword.get(opts, :traffic_raw_body, false),
      all_headers: Keyword.get(opts, :traffic_all_headers, false)
    }

    event_opts = %{
      detail: parse_event_detail(opts[:events], logs_enabled or traffic_enabled)
    }

    bt_opts = %{
      coredumps: Keyword.get(opts, :coredumps, true),
      threads: parse_threads_opt(opts[:threads]),
      max_frames: Keyword.get(opts, :backtrace_frames, 20),
      disassembly: Keyword.get(opts, :disassembly, false)
    }

    display = %{log: log_opts, traffic: traffic_opts, event: event_opts, backtrace: bt_opts}

    if selected == [] do
      Mix.shell().info(colorize("No matching issues.", :yellow, color))
    else
      Enum.each(selected, fn {issue, idx} ->
        print_issue_detail(issue, idx, color, display)
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

  defp parse_list_filter(nil), do: nil

  defp parse_list_filter(spec),
    do: spec |> String.split(",", trim: true) |> Enum.map(&String.trim/1)

  defp parse_status_filter(nil), do: nil

  defp parse_status_filter(spec) do
    case String.split(spec, "-") do
      [single] -> {String.to_integer(single), String.to_integer(single)}
      [min, max] -> {String.to_integer(min), String.to_integer(max)}
    end
  end

  defp parse_body_limit(nil), do: 200

  defp parse_body_limit("unlimited"), do: :unlimited

  defp parse_body_limit(val) do
    case Integer.parse(val) do
      {0, ""} ->
        :unlimited

      {n, ""} when n > 0 ->
        n

      _ ->
        Mix.raise(
          "Invalid --traffic-body-limit: #{val}. Use a positive integer or \"unlimited\"."
        )
    end
  end

  @valid_event_details %{"none" => :none, "basic" => :basic, "full" => :full}

  def parse_event_detail(nil, any_stream_enabled) do
    if any_stream_enabled, do: :basic, else: :none
  end

  def parse_event_detail(level, _any_stream_enabled) do
    Map.get(@valid_event_details, level) ||
      Mix.raise(
        "Unknown --events level: #{level}. Valid: #{@valid_event_details |> Map.keys() |> Enum.join(", ")}"
      )
  end

  @log_implicit_enable_keys ~w(log_servers log_window log_min_level log_exclude)a

  defp logs_enabled?(opts) do
    opts[:logs] || Enum.any?(@log_implicit_enable_keys, &(opts[&1] != nil))
  end

  @traffic_implicit_enable_keys ~w(traffic_servers traffic_window traffic_methods traffic_endpoints traffic_status)a

  defp traffic_enabled?(opts) do
    opts[:traffic] || Enum.any?(@traffic_implicit_enable_keys, &(opts[&1] != nil))
  end

  # --- Issue detail rendering ---

  defp print_issue_detail(issue, idx, color, display) do
    bar = String.duplicate("─", 80)
    Mix.shell().info("\n#{colorize(bar, :faint, color)}")
    print_issue_header(issue, idx, color)
    Body.print(issue, color, display.backtrace)
    print_issue_appendices(issue, display, color)
  end

  defp print_issue_appendices(issue, display, color) do
    if display.log.enabled or display.traffic.enabled or display.event.detail != :none do
      Streams.print(issue, display, color)
    end
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
end
