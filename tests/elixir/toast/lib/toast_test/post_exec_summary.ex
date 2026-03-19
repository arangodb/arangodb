defmodule ToastTest.PostExecSummary do
  @moduledoc false

  import ToastTest.Formatting

  alias ToastTest.{IssueFormatting, SuiteResult}

  @issue_types_by_severity [:test_failure, :sanitizer_report, :crash, :timeout]

  @spec print(SuiteResult.t()) :: :ok
  def print(%SuiteResult{issues: issues, warnings: warnings}) do
    print_issues(issues)
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
    IO.puts("")
    IO.puts(IO.ANSI.yellow() <> String.duplicate("!", 80) <> IO.ANSI.reset())
    IO.puts(IO.ANSI.yellow() <> IO.ANSI.bright() <> " WARNINGS" <> IO.ANSI.reset())
    IO.puts(IO.ANSI.yellow() <> String.duplicate("!", 80) <> IO.ANSI.reset())

    for warning <- warnings do
      IO.puts("")
      IO.puts("  " <> IO.ANSI.yellow() <> warning <> IO.ANSI.reset())
    end
  end

  defp print_section(type, issues, colors) do
    {label, color} = section_header(type, length(issues))
    bar = String.duplicate("\u2550", 80)

    IO.puts("\n#{colorize(bar, color, colors)}")
    IO.puts(colorize(" #{label}", color, colors))
    IO.puts(colorize(bar, color, colors))

    Enum.reduce(issues, 1, fn issue, counter ->
      print_issue(type, issue, counter, colors)
    end)
  end

  defp section_header(:test_failure, count), do: {"TEST FAILURES (#{count})", :red}
  defp section_header(:sanitizer_report, count), do: {"SANITIZER REPORTS (#{count})", :yellow}
  defp section_header(:crash, count), do: {"CRASHES (#{count})", :red}
  defp section_header(:timeout, count), do: {"TIMEOUTS (#{count})", :red}

  # --- Test failures ---

  defp print_issue(:test_failure, issue, counter, _colors) do
    %{detail: %{test: %ExUnit.Test{state: {:failed, failures}} = test}} = issue

    formatted =
      ExUnit.Formatter.format_test_failure(
        test,
        failures,
        counter,
        :infinity,
        &formatter_cb/2
      )

    IO.puts("\n#{formatted}")
    counter + 1
  end

  # --- Sanitizer reports ---

  defp print_issue(:sanitizer_report, issue, counter, colors) do
    %{scope: scope, detail: %{server: server}} = issue

    print_attribution(scope, server, colors)
    print_indented(IssueFormatting.format_sanitizer(issue), "    ")

    counter + 1
  end

  # --- Crashes ---

  defp print_issue(:crash, issue, counter, colors) do
    %{scope: scope, detail: detail} = issue
    %{server: server} = detail

    print_attribution(scope, server, colors)
    print_crash_info(detail, colors)
    print_crash_detail(detail, colors)

    counter + 1
  end

  # --- Timeouts ---

  defp print_issue(:timeout, issue, counter, colors) do
    %{detail: %{source: source, reason: reason, servers: servers}} = issue

    label = IssueFormatting.timeout_source_label(source)
    IO.puts("\n  #{colorize("[#{label}] #{reason}", :red, colors)}")

    if servers != [] do
      server_count = length(servers)
      server_word = if server_count == 1, do: "server", else: "servers"
      IO.puts("    Aborted #{server_count} #{server_word}:")

      Enum.each(servers, fn server ->
        pid_part = if server.os_pid, do: " (PID #{server.os_pid})", else: ""
        IO.puts("    #{colorize("#{server.server_id}#{pid_part}", :cyan, colors)}")

        if server.log_file do
          IO.puts(IO.ANSI.format([:blue, "      Log: #{server.log_file}", :reset]))
        end

        if server[:coredump] do
          IO.puts(IO.ANSI.format([:blue, "      Coredump: #{server.coredump}", :reset]))
        end
      end)
    end

    counter + 1
  end

  # --- Crash details ---

  defp print_crash_info(%{crash_info: _} = detail, colors) do
    case IssueFormatting.format_crash_info(detail) do
      nil -> :ok
      text -> IO.puts("    #{colorize(text, :red, colors)}")
    end
  end

  defp print_crash_info(_detail, _colors), do: :ok

  # Prefer the crash log output over the coredump backtrace — the log
  # captures the original CPU context from the signal handler, which may
  # differ from the coredump (see killProcess / SA_RESETHAND re-raise).
  defp print_crash_detail(%{crash_lines: crash_lines} = detail, _colors)
       when is_binary(crash_lines) do
    IssueFormatting.format_crash_detail(detail)
    |> print_indented("    ")
  end

  defp print_crash_detail(%{coredumps: [_ | _]} = detail, _colors) do
    IssueFormatting.format_crash_detail(detail)
    |> print_indented("    ")
  end

  defp print_crash_detail(detail, colors) do
    IO.puts("    #{colorize("No crash details available.", :faint, colors)}")

    case IssueFormatting.format_log_path(detail) do
      nil -> :ok
      text -> IO.puts(IO.ANSI.format([:blue, "    #{text}", :reset]))
    end
  end

  # --- Attribution ---

  defp print_attribution(scope, server, colors) do
    case IssueFormatting.format_scope(scope) do
      nil -> IO.puts("\n  #{colorize(server, :cyan, colors)}")
      label -> IO.puts("\n  #{colorize(server, :cyan, colors)} \u2014 #{label}")
    end
  end

  # --- Helpers ---

  defp print_indented(nil, _prefix), do: :ok

  defp print_indented(text, prefix) do
    text
    |> String.split("\n")
    |> Enum.each(&IO.puts("#{prefix}#{&1}"))
  end
end
