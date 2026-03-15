defmodule ToastTest.PostExecSummaryTest do
  use ExUnit.Case, async: true

  import ExUnit.CaptureIO

  alias ToastTest.{SuiteResult, PostExecSummary}

  defp suite_result(issues) do
    %SuiteResult{
      suite: "smoke",
      started_at: ~U[2026-03-09 10:00:00Z],
      finished_at: ~U[2026-03-09 10:05:00Z],
      times_us: %{async: 0, load: 0, run: 300_000_000},
      modules: %{},
      issues: issues,
      events: %{}
    }
  end

  defp test_failure_issue do
    %{
      type: :test_failure,
      scope: {:test, SomeModule, :"test something"},
      confidence: nil,
      detail: %{
        test: %ExUnit.Test{
          name: :"test something",
          module: SomeModule,
          state:
            {:failed,
             [
               {:error,
                %ExUnit.AssertionError{
                  left: 1,
                  right: 2,
                  message: "Assertion with == failed",
                  expr: {:==, [], [1, 2]}
                }, []}
             ]},
          tags: %{file: "test/some_test.exs", line: 10, test_type: :test},
          time: 1234,
          logs: ""
        }
      }
    }
  end

  defp sanitizer_issue(opts \\ []) do
    line_count = Keyword.get(opts, :lines, 5)
    scope = Keyword.get(opts, :scope, {:test, SomeModule, :"test something"})

    report =
      Enum.map_join(1..line_count, "\n", fn i ->
        "ERROR line #{i}: AddressSanitizer detail"
      end)

    %{
      type: :sanitizer_report,
      scope: scope,
      confidence: :high,
      detail: %{server: "agent1", report: report}
    }
  end

  defp crash_info(opts \\ []) do
    %Toast.Process.CrashInfo{
      signal: Keyword.get(opts, :signal, 11),
      exit_status: Keyword.get(opts, :exit_status, 139),
      timestamp: Keyword.get(opts, :timestamp, ~U[2026-03-09 10:00:05Z]),
      os_pid: Keyword.get(opts, :os_pid, 22788)
    }
  end

  defp crash_issue_with_coredump(opts \\ []) do
    scope = Keyword.get(opts, :scope, {:test, SomeModule, :"test something"})

    %{
      type: :crash,
      scope: scope,
      confidence: :high,
      detail: %{
        server: "agent1",
        crash_info: crash_info(),
        coredumps: [
          %{
            path: "/tmp/core.1234",
            signal: "SIGSEGV",
            threads: [
              %{
                thread_id: "1",
                name: "main",
                backtrace: "#0 foo() at foo.cpp:1\n#1 bar() at bar.cpp:2"
              }
            ]
          }
        ],
        log_file: "/tmp/arangodb/agent1.log"
      }
    }
  end

  defp crash_issue_with_logs do
    %{
      type: :crash,
      scope: :suite,
      confidence: nil,
      detail: %{
        server: "agent1",
        crash_info: crash_info(),
        logs: build_log_lines(),
        log_file: "/tmp/arangodb/agent1.log"
      }
    }
  end

  # By the time logs reach PostExecSummary, they contain only {crash} lines
  # (extracted by Enrichment.Logs.extract_crash_lines/1)
  defp build_log_lines do
    [
      "2026-03-09T10:00:05Z [12345-30] S FATAL [a7902] {crash} caught signal 11 (SIGSEGV)",
      "2026-03-09T10:00:05Z [12345-30] S FATAL [a7903] {crash} Hello crash handler",
      "2026-03-09T10:00:05Z [12345-30] S INFO [ded81] {crash} available physical memory: 1073741824",
      "2026-03-09T10:00:05Z [12345-30] S INFO [c962b] {crash} Backtrace of thread 30 [SchedWorker]",
      "2026-03-09T10:00:05Z [12345-30] S INFO [308c3] {crash} frame  1 0x555555678abc some_func (+0x42)",
      "2026-03-09T10:00:05Z [12345-30] S INFO [308c3] {crash} frame  2 0x555555679def other_func (+0x10)"
    ]
    |> Enum.join("\n")
  end

  test "prints nothing when no issues" do
    output = capture_io(fn -> PostExecSummary.print(suite_result([])) end)

    assert output == ""
  end

  test "prints test failures with ExUnit output" do
    output = capture_io(fn -> PostExecSummary.print(suite_result([test_failure_issue()])) end)

    assert output =~ "TEST FAILURES (1)"
    assert output =~ "Assertion with == failed"
  end

  test "prints sanitizer reports with attribution and truncation" do
    issue = sanitizer_issue(lines: 25)
    output = capture_io(fn -> PostExecSummary.print(suite_result([issue])) end)

    assert output =~ "SANITIZER REPORTS (1)"
    assert output =~ "agent1"
    assert output =~ ~s(SomeModule > "something")
    assert output =~ "ERROR line 1:"
    assert output =~ "ERROR line 15:"
    refute output =~ "ERROR line 16:"
    assert output =~ "... (10 more lines)"
  end

  test "prints crashes with crash info, backtrace, and log path" do
    output =
      capture_io(fn -> PostExecSummary.print(suite_result([crash_issue_with_coredump()])) end)

    assert output =~ "CRASHES (1)"
    assert output =~ "agent1: PID 22788"
    assert output =~ "signal: SIGSEGV (11)"
    assert output =~ "exit_status: 139"
    assert output =~ "at: 2026-03-09T10:00:05Z"
    assert output =~ "#0 foo() at foo.cpp:1"
    assert output =~ "#1 bar() at bar.cpp:2"
    assert output =~ "Log: /tmp/arangodb/agent1.log"
  end

  test "prints crashes with log fallback when no coredumps" do
    output = capture_io(fn -> PostExecSummary.print(suite_result([crash_issue_with_logs()])) end)

    assert output =~ "CRASHES (1)"
    assert output =~ "agent1: PID 22788"
    assert output =~ "signal: SIGSEGV (11)"
    assert output =~ "exit_status: 139"
    assert output =~ "caught signal 11"
    assert output =~ "Backtrace of thread 30"
    assert output =~ "some_func"
    assert output =~ "Log: /tmp/arangodb/agent1.log"
  end

  test "shows fallback message when crash has no logs" do
    issue = %{
      type: :crash,
      scope: :suite,
      confidence: nil,
      detail: %{
        server: "agent1",
        crash_info: crash_info(),
        log_file: "/tmp/arangodb/agent1.log"
      }
    }

    output = capture_io(fn -> PostExecSummary.print(suite_result([issue])) end)
    assert output =~ "agent1: PID 22788"
    assert output =~ "signal: SIGSEGV (11)"
    assert output =~ "No crash details available."
    assert output =~ "Log: /tmp/arangodb/agent1.log"
  end

  test "prints startup timeout with aborted servers, log paths, and coredump paths" do
    issue = %{
      type: :timeout,
      scope: :suite,
      confidence: :high,
      detail: %{
        source: :startup_timeout,
        reason: "Startup timeout — deployment did not become ready in time",
        timestamp: ~U[2026-03-09 10:05:00Z],
        servers: [
          %{
            server_id: "agent1",
            os_pid: 22788,
            log_file: "/tmp/agent1.log",
            coredump: "/tmp/core.22788"
          },
          %{server_id: "dbserver1", os_pid: 22790, log_file: "/tmp/dbserver1.log", coredump: nil},
          %{
            server_id: "coordinator1",
            os_pid: 22792,
            log_file: "/tmp/coordinator1.log",
            coredump: "/tmp/core.22792"
          }
        ]
      }
    }

    output = capture_io(fn -> PostExecSummary.print(suite_result([issue])) end)

    assert output =~ "TIMEOUTS (1)"
    assert output =~ "[Startup Timeout]"
    assert output =~ "Startup timeout"
    assert output =~ "Aborted 3 servers:"

    assert output =~ "agent1 (PID 22788)"
    assert output =~ "Log: /tmp/agent1.log"
    assert output =~ "Coredump: /tmp/core.22788"

    assert output =~ "dbserver1 (PID 22790)"
    assert output =~ "Log: /tmp/dbserver1.log"
    refute output =~ "Coredump: /tmp/core.22790"

    assert output =~ "coordinator1 (PID 22792)"
    assert output =~ "Coredump: /tmp/core.22792"
  end

  test "prints test timeout without server list" do
    issue = %{
      type: :timeout,
      scope: :suite,
      confidence: :high,
      detail: %{
        source: :test_timeout,
        reason: "Suite timeout exceeded",
        timestamp: ~U[2026-03-09 10:05:00Z],
        servers: []
      }
    }

    output = capture_io(fn -> PostExecSummary.print(suite_result([issue])) end)

    assert output =~ "[Test Timeout]"
    assert output =~ "Suite timeout exceeded"
    refute output =~ "Aborted"
  end

  test "prints shutdown timeout with escalated server" do
    issue = %{
      type: :timeout,
      scope: :suite,
      confidence: :high,
      detail: %{
        source: :shutdown_timeout,
        reason: "Shutdown timeout — server(s) did not respond to SIGTERM",
        timestamp: ~U[2026-03-09 10:05:00Z],
        servers: [
          %{server_id: "single", os_pid: 12345, log_file: "/tmp/single.log", coredump: nil}
        ]
      }
    }

    output = capture_io(fn -> PostExecSummary.print(suite_result([issue])) end)

    assert output =~ "[Shutdown Timeout]"
    assert output =~ "Aborted 1 server:"
    assert output =~ "single (PID 12345)"
  end

  test "orders sections by severity with timeouts last" do
    issues = [
      crash_issue_with_coredump(),
      test_failure_issue(),
      sanitizer_issue(),
      %{
        type: :timeout,
        scope: :suite,
        confidence: :high,
        detail: %{
          source: :test_timeout,
          reason: "Suite timeout exceeded",
          timestamp: ~U[2026-03-09 10:05:00Z],
          servers: []
        }
      }
    ]

    output = capture_io(fn -> PostExecSummary.print(suite_result(issues)) end)

    failure_pos = :binary.match(output, "TEST FAILURES") |> elem(0)
    sanitizer_pos = :binary.match(output, "SANITIZER REPORTS") |> elem(0)
    crash_pos = :binary.match(output, "CRASHES") |> elem(0)
    timeout_pos = :binary.match(output, "TIMEOUTS") |> elem(0)

    assert failure_pos < sanitizer_pos
    assert sanitizer_pos < crash_pos
    assert crash_pos < timeout_pos
  end

  test "formats scope attribution" do
    suite_crash = %{crash_issue_with_coredump() | scope: :suite}
    module_crash = %{crash_issue_with_coredump() | scope: {:module, SomeModule}}
    test_crash = crash_issue_with_coredump(scope: {:test, SomeModule, :"test foo"})

    suite_output = capture_io(fn -> PostExecSummary.print(suite_result([suite_crash])) end)
    module_output = capture_io(fn -> PostExecSummary.print(suite_result([module_crash])) end)
    test_output = capture_io(fn -> PostExecSummary.print(suite_result([test_crash])) end)

    assert suite_output =~ "agent1"
    refute suite_output =~ "SomeModule"

    assert module_output =~ "SomeModule"

    assert test_output =~ "SomeModule"
    assert test_output =~ ~s(> "foo")
  end
end
