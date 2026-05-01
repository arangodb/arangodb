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

defmodule ToastTest.Formatting.PostExecSummary do
  @moduledoc false

  import ToastTest.Formatting

  alias ToastTest.Formatting.{Color, Issues, Utils}
  alias ToastTest.SuiteResult

  @issue_types_by_severity [:test_failure, :sanitizer_report, :crash, :timeout]

  @spec print(SuiteResult.t()) :: :ok
  def print(%SuiteResult{issues: issues, warnings: warnings} = result) do
    index = Issues.build_coredump_index(result.coredumps)

    issues
    |> Issues.resolve_coredumps(index)
    |> Enum.map(&Issues.attach_test_location(&1, result.modules))
    |> print_issues()

    print_warnings(warnings)
    :ok
  end

  defp print_issues([]), do: :ok

  defp print_issues(issues) do
    colors = IO.ANSI.enabled?()
    grouped = Enum.group_by(issues, & &1.type)

    @issue_types_by_severity
    |> Enum.filter(&Map.has_key?(grouped, &1))
    |> Enum.each(fn type ->
      issues_of_type = Map.fetch!(grouped, type)
      print_section(type, issues_of_type, colors)
    end)
  end

  defp print_warnings([]), do: :ok

  defp print_warnings(warnings) do
    colors = IO.ANSI.enabled?()
    Utils.print_header("WARNINGS", colors, Color.warning())

    for warning <- warnings do
      IO.puts("  " <> colorize(warning, :yellow, colors) <> "\n")
    end
  end

  defp print_section(type, issues, colors) do
    {label, color} = section_header(type, length(issues))
    Utils.print_header(label, colors, color)

    Enum.reduce(issues, 1, fn issue, counter ->
      print_issue(type, issue, counter, colors)
    end)
  end

  defp section_header(:test_failure, count), do: {"TEST FAILURES (#{count})", Color.failure()}
  defp section_header(:crash, count), do: {"CRASHES (#{count})", Color.crash()}
  defp section_header(:timeout, count), do: {"TIMEOUTS (#{count})", Color.timeout()}

  defp section_header(:sanitizer_report, count),
    do: {"SANITIZER REPORTS (#{count})", Color.sanitizer()}

  defp print_issue(:test_failure, issue, counter, _colors) do
    %{detail: %{test: %ExUnit.Test{state: {:failed, failures}} = test}} = issue

    ExUnit.Formatter.format_test_failure(
      test,
      failures,
      counter,
      :infinity,
      &formatter_cb/2
    )
    |> IO.puts()

    counter + 1
  end

  defp print_issue(:sanitizer_report, issue, counter, colors) do
    %{detail: %{server: server}} = issue
    print_attribution(issue, server, colors)
    print_indented(Issues.format_sanitizer(issue), "    ")
    counter + 1
  end

  defp print_issue(:crash, issue, counter, colors) do
    %{detail: %{server: server} = detail} = issue
    print_attribution(issue, server, colors)
    print_crash_info(detail, colors)
    print_crash_detail(detail, colors)
    counter + 1
  end

  defp print_issue(:timeout, issue, counter, colors) do
    %{detail: %{source: source, reason: reason, servers: servers}} = issue
    label = Issues.timeout_source_label(source)
    IO.puts("\n  #{colorize("[#{label}] #{reason}", :red, colors)}")

    if servers != [] do
      server_count = length(servers)
      IO.puts("    Aborted #{server_count} #{Toast.Utils.pluralize(server_count, "server")}:")
      Enum.each(servers, &print_server_detail(&1, colors))
    end

    counter + 1
  end

  defp print_server_detail(server, colors) do
    pid_part = if server.os_pid, do: " (PID #{server.os_pid})", else: ""
    IO.puts("    #{colorize("#{server.server_id}#{pid_part}", :cyan, colors)}")

    if server.log_file do
      IO.puts(IO.ANSI.format([:blue, "      Log: #{server.log_file}", :reset]))
    end

    if server[:coredump] do
      IO.puts(IO.ANSI.format([:blue, "      Coredump: #{server.coredump}", :reset]))
    end
  end

  defp print_crash_info(%{crash_info: _} = detail, colors) do
    case Issues.format_crash_info(detail) do
      nil -> :ok
      text -> IO.puts("    #{colorize(text, :red, colors)}")
    end
  end

  defp print_crash_info(_detail, _colors), do: :ok

  # Prefer the crash log output over the coredump backtrace — the log
  # captures the original CPU context from the signal handler, which may
  # differ from the coredump (see killProcess / SA_RESETHAND re-raise).
  defp print_crash_detail(detail, colors) do
    case Issues.format_crash_detail(detail) do
      nil -> IO.puts("    #{colorize("No crash details available.", :faint, colors)}")
      text -> print_indented(text, "    ")
    end

    print_blue(Issues.format_coredump_path(detail))
    print_blue(Issues.format_log_path(detail))
  end

  defp print_blue(nil), do: :ok
  defp print_blue(text), do: IO.puts(IO.ANSI.format([:blue, "    #{text}", :reset]))

  defp print_attribution(issue, server, colors) do
    label = format_scope_with_location(issue)

    case label do
      nil -> IO.puts("  #{colorize(server, :cyan, colors)}")
      _ -> IO.puts("  #{colorize(server, :cyan, colors)} \u2014 #{label}")
    end
  end

  defp format_scope_with_location(%{scope: scope, test_location: loc}) when is_binary(loc) do
    "#{Issues.format_scope(scope)} (#{loc})"
  end

  defp format_scope_with_location(%{scope: scope}) do
    Issues.format_scope(scope)
  end

  defp print_indented(text, prefix) do
    text
    |> String.split("\n")
    |> Enum.each(&IO.puts("#{prefix}#{&1}"))
  end
end
