defmodule ToastTest.PostExecSummary do
  @moduledoc false

  import ToastTest.Formatting

  alias ToastTest.SuiteResult

  @max_sanitizer_lines 15
  @max_backtrace_frames 20
  @max_crash_log_lines 15

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
    %{scope: scope, detail: %{server: server, report: report}} = issue

    print_attribution(scope, server, colors)
    print_truncated(report, @max_sanitizer_lines, "    ")

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

    label = timeout_source_label(source)
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

  defp timeout_source_label(:startup_timeout), do: "Startup Timeout"
  defp timeout_source_label(:shutdown_timeout), do: "Shutdown Timeout"
  defp timeout_source_label(:test_timeout), do: "Test Timeout"
  defp timeout_source_label(:global_timeout), do: "Global Timeout"
  defp timeout_source_label(other), do: "Timeout: #{other}"

  # --- Crash details ---

  defp print_crash_info(%{server: server, crash_info: info}, colors) do
    parts =
      [
        format_pid(info.os_pid),
        format_signal(info.signal),
        format_exit_status(info.exit_status),
        format_timestamp(info.timestamp)
      ]
      |> Toast.Utils.compact()

    IO.puts("    #{colorize("#{server}: #{Enum.join(parts, "  ")}", :red, colors)}")
  end

  defp print_crash_info(_detail, _colors), do: :ok

  defp format_pid(nil), do: nil
  defp format_pid(pid), do: "PID #{pid}"

  defp format_signal(nil), do: nil

  defp format_signal(sig) do
    case :exec.signal(sig) do
      name when is_atom(name) -> "signal: #{name |> Atom.to_string() |> String.upcase()} (#{sig})"
      _ -> "signal: #{sig}"
    end
  end

  defp format_exit_status(nil), do: nil
  defp format_exit_status(status), do: "exit_status: #{status}"

  defp format_timestamp(%DateTime{} = ts), do: "at: #{DateTime.to_iso8601(ts)}"
  defp format_timestamp(_), do: nil

  defp print_crash_detail(%{coredumps: [coredump | _]} = detail, colors) do
    print_coredump_backtrace(coredump, colors)
    print_coredump_path(coredump)
    print_log_path(detail)
  end

  defp print_crash_detail(%{logs: logs} = detail, _colors) when is_binary(logs) do
    print_truncated(logs, @max_crash_log_lines, "    ")
    print_log_path(detail)
  end

  defp print_crash_detail(detail, colors) do
    IO.puts("    #{colorize("No crash details available.", :faint, colors)}")
    print_log_path(detail)
  end

  defp print_coredump_backtrace(coredump, _colors) do
    case coredump.threads do
      [thread | _] ->
        lines = String.split(thread.backtrace, "\n")
        shown = Enum.take(lines, @max_backtrace_frames)
        Enum.each(shown, &IO.puts("    #{&1}"))

        remaining = length(lines) - length(shown)
        if remaining > 0, do: IO.puts("    ...")

      _ ->
        :ok
    end
  end

  defp print_coredump_path(%{path: path}) when is_binary(path) do
    IO.puts(IO.ANSI.format([:blue, "    Coredump: #{path}", :reset]))
  end

  defp print_coredump_path(_), do: :ok

  defp print_log_path(%{log_file: path}) when is_binary(path) do
    IO.puts(IO.ANSI.format([:blue, "    Log: #{path}", :reset]))
  end

  defp print_log_path(_), do: :ok

  # --- Shared truncation ---

  defp print_truncated(text, max_lines, prefix) do
    lines = String.split(text, "\n")
    shown = Enum.take(lines, max_lines)
    Enum.each(shown, &IO.puts("#{prefix}#{&1}"))

    remaining = length(lines) - length(shown)
    if remaining > 0, do: IO.puts("#{prefix}... (#{remaining} more lines)")
  end

  # --- Attribution ---

  defp print_attribution(scope, server, colors) do
    case format_scope(scope) do
      nil -> IO.puts("\n  #{colorize(server, :cyan, colors)}")
      label -> IO.puts("\n  #{colorize(server, :cyan, colors)} \u2014 #{label}")
    end
  end

  defp format_scope(:suite), do: nil
  defp format_scope({:module, mod}), do: inspect(mod)

  defp format_scope({:test, mod, name}) do
    short_name = name |> to_string() |> String.replace_prefix("test ", "")
    "#{inspect(mod)} > \"#{short_name}\""
  end
end
