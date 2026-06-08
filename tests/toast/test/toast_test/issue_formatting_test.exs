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

defmodule ToastTest.Formatting.IssuesTest do
  use ExUnit.Case, async: true

  alias ToastTest.Formatting.Issues, as: IssueFormatting

  # --- build_coredump_index/1 ---

  describe "build_coredump_index/1" do
    test "builds path-keyed map from coredump reports" do
      reports = [
        %{core_path: "/tmp/core.1234", threads: []},
        %{core_path: "/tmp/core.5678", threads: []}
      ]

      index = IssueFormatting.build_coredump_index(reports)

      assert map_size(index) == 2
      assert index["/tmp/core.1234"] == %{core_path: "/tmp/core.1234", threads: []}
      assert index["/tmp/core.5678"] == %{core_path: "/tmp/core.5678", threads: []}
    end

    test "empty list returns empty map" do
      assert IssueFormatting.build_coredump_index([]) == %{}
    end

    test "duplicate paths keep last entry" do
      reports = [
        %{core_path: "/tmp/core.1", data: :first},
        %{core_path: "/tmp/core.1", data: :second}
      ]

      index = IssueFormatting.build_coredump_index(reports)
      assert map_size(index) == 1
      assert index["/tmp/core.1"].data == :second
    end
  end

  # --- resolve_coredumps/2 ---

  describe "resolve_coredumps/2" do
    test "resolves coredump_paths on crash issues" do
      report = %{core_path: "/tmp/core.1", threads: []}
      index = %{"/tmp/core.1" => report}

      issues = [
        %{type: :crash, detail: %{coredump_paths: ["/tmp/core.1"]}}
      ]

      [resolved] = IssueFormatting.resolve_coredumps(issues, index)
      assert resolved.detail.coredumps == [report]
    end

    test "skips paths not in index" do
      index = %{}

      issues = [
        %{type: :crash, detail: %{coredump_paths: ["/tmp/missing.core"]}}
      ]

      [resolved] = IssueFormatting.resolve_coredumps(issues, index)
      assert resolved.detail.coredumps == []
    end

    test "handles crash issues with no coredump_paths key" do
      issues = [%{type: :crash, detail: %{}}]
      [resolved] = IssueFormatting.resolve_coredumps(issues, %{})
      assert resolved.detail.coredumps == []
    end

    test "passes through non-crash issues unchanged" do
      issue = %{type: :sanitizer_report, detail: %{report: "asan"}}
      [result] = IssueFormatting.resolve_coredumps([issue], %{})
      assert result == issue
    end

    test "handles mixed issue types" do
      report = %{core_path: "/tmp/core.1", threads: []}
      index = %{"/tmp/core.1" => report}

      issues = [
        %{type: :crash, detail: %{coredump_paths: ["/tmp/core.1"]}},
        %{type: :timeout, detail: %{reason: "timed out"}}
      ]

      [crash, timeout] = IssueFormatting.resolve_coredumps(issues, index)
      assert crash.detail.coredumps == [report]
      assert timeout == Enum.at(issues, 1)
    end
  end

  # --- format_sanitizer/1 ---

  describe "format_sanitizer/1" do
    test "formats with attribution and report" do
      issue = %{
        scope: :suite,
        detail: %{server: "dbserver1", report: "ERROR: LeakSanitizer"}
      }

      result = IssueFormatting.format_sanitizer(issue)
      assert result =~ "dbserver1"
      assert result =~ "ERROR: LeakSanitizer"
    end

    test "truncates report beyond 15 lines" do
      long_report = Enum.map_join(1..20, "\n", &"line #{&1}")

      issue = %{
        scope: :suite,
        detail: %{server: "dbserver1", report: long_report}
      }

      result = IssueFormatting.format_sanitizer(issue)
      assert result =~ "... (5 more lines)"
      refute result =~ "line 16"
    end

    test "handles nil report" do
      issue = %{scope: :suite, detail: %{server: "dbserver1"}}
      result = IssueFormatting.format_sanitizer(issue)
      assert result == "dbserver1"
    end
  end

  # --- format_crash/1 ---

  describe "format_crash/1" do
    test "formats minimal crash (no optional fields)" do
      issue = %{scope: :suite, detail: %{server: "coordinator1"}}
      result = IssueFormatting.format_crash(issue)
      assert result == "coordinator1"
    end

    test "formats crash with crash_lines" do
      issue = %{
        scope: :suite,
        detail: %{
          server: "coordinator1",
          crash_lines: "segfault at address 0x0"
        }
      }

      result = IssueFormatting.format_crash(issue)
      assert result =~ "coordinator1"
      assert result =~ "segfault at address 0x0"
    end

    test "formats crash with log_file" do
      issue = %{
        scope: :suite,
        detail: %{server: "coordinator1", log_file: "/var/log/arangod.log"}
      }

      result = IssueFormatting.format_crash(issue)
      assert result =~ "Log: /var/log/arangod.log"
    end

    test "formats crash with core_path" do
      issue = %{
        scope: :suite,
        detail: %{server: "coordinator1", core_path: "/tmp/core.1234"}
      }

      result = IssueFormatting.format_crash(issue)
      assert result =~ "Coredump: /tmp/core.1234"
    end
  end

  # --- format_crash_info/1 ---

  describe "format_crash_info/1" do
    test "formats all crash info fields" do
      ts = ~U[2026-03-09 10:00:00Z]

      detail = %{
        server: "dbserver1",
        crash_info: %{os_pid: 1234, signal: 11, exit_status: 139, timestamp: ts}
      }

      result = IssueFormatting.format_crash_info(detail)
      assert result =~ "dbserver1: "
      assert result =~ "PID 1234"
      assert result =~ "signal:"
      assert result =~ "exit_status: 139"
      assert result =~ "at: 2026-03-09T10:00:00Z"
    end

    test "formats with only server and partial crash_info" do
      detail = %{server: "dbserver1", crash_info: %{os_pid: 42}}
      result = IssueFormatting.format_crash_info(detail)
      assert result == "dbserver1: PID 42"
    end

    test "returns nil for detail without server and crash_info" do
      assert IssueFormatting.format_crash_info(%{}) == nil
    end
  end

  # --- format_crash_detail/1 ---

  describe "format_crash_detail/1" do
    test "returns truncated crash_lines when present" do
      lines = Enum.map_join(1..20, "\n", &"line #{&1}")
      result = IssueFormatting.format_crash_detail(%{crash_lines: lines})
      assert result =~ "line 1"
      assert result =~ "... (5 more lines)"
      refute result =~ "line 16"
    end

    test "prefers crash_lines over coredumps" do
      detail = %{
        crash_lines: "segfault",
        coredumps: [%{threads: [%{frames: [%{function: "main"}]}]}]
      }

      result = IssueFormatting.format_crash_detail(detail)
      assert result == "segfault"
    end

    test "falls back to coredump backtrace when no crash_lines" do
      detail = %{
        coredumps: [
          %{threads: [%{frames: [%{function: "main", file: "main.cpp", line: 10}]}]}
        ]
      }

      result = IssueFormatting.format_crash_detail(detail)
      assert result =~ "#0 main at main.cpp:10"
    end

    test "returns nil when neither crash_lines nor coredumps" do
      assert IssueFormatting.format_crash_detail(%{}) == nil
    end

    test "returns nil for non-binary crash_lines" do
      assert IssueFormatting.format_crash_detail(%{crash_lines: 42}) == nil
    end
  end

  # --- format_timeout/1 ---

  describe "format_timeout/1" do
    test "formats timeout with server details" do
      issue = %{
        detail: %{
          source: :test_timeout,
          reason: "test exceeded 60s",
          servers: [
            %{server_id: "coordinator1", os_pid: 1234, log_file: "/tmp/c1.log", coredump: nil}
          ]
        }
      }

      result = IssueFormatting.format_timeout(issue)
      assert result =~ "[Test Timeout] test exceeded 60s"
      assert result =~ "coordinator1 (PID 1234)"
      assert result =~ "Log: /tmp/c1.log"
    end

    test "formats timeout with multiple servers" do
      issue = %{
        detail: %{
          source: :startup_timeout,
          reason: "cluster failed to start",
          servers: [
            %{server_id: "coordinator1", os_pid: 100, log_file: nil, coredump: nil},
            %{server_id: "dbserver1", os_pid: 200, log_file: nil, coredump: "/tmp/core.200"}
          ]
        }
      }

      result = IssueFormatting.format_timeout(issue)
      assert result =~ "[Startup Timeout]"
      assert result =~ "coordinator1 (PID 100)"
      assert result =~ "dbserver1 (PID 200)"
      assert result =~ "Coredump: /tmp/core.200"
    end

    test "formats timeout with no servers" do
      issue = %{
        detail: %{source: :global_timeout, reason: "exceeded 10m"}
      }

      result = IssueFormatting.format_timeout(issue)
      assert result == "[Global Timeout] exceeded 10m"
    end

    test "server without os_pid omits PID part" do
      issue = %{
        detail: %{
          source: :shutdown_timeout,
          reason: "shutdown stuck",
          servers: [%{server_id: "coordinator1", os_pid: nil, log_file: nil, coredump: nil}]
        }
      }

      result = IssueFormatting.format_timeout(issue)
      assert result =~ "  coordinator1"
      refute result =~ "PID"
    end
  end

  # --- format_attribution/2 ---

  describe "format_attribution/2" do
    test "suite scope returns server only" do
      assert IssueFormatting.format_attribution(:suite, "dbserver1") == "dbserver1"
    end

    test "module scope returns server with module label" do
      result = IssueFormatting.format_attribution({:module, MyApp.SomeTest}, "coordinator1")
      assert result == "coordinator1 \u2014 MyApp.SomeTest"
    end

    test "test scope returns server with module and test label" do
      result =
        IssueFormatting.format_attribution(
          {:test, MyApp.SomeTest, :"test my feature works"},
          "coordinator1"
        )

      assert result == "coordinator1 \u2014 MyApp.SomeTest > \"my feature works\""
    end

    test "nil server with suite scope" do
      assert IssueFormatting.format_attribution(:suite, nil) == nil
    end
  end

  # --- format_scope/1 ---

  describe "format_scope/1" do
    test "suite returns nil" do
      assert IssueFormatting.format_scope(:suite) == nil
    end

    test "module tuple returns inspected module" do
      assert IssueFormatting.format_scope({:module, MyApp.Test}) == "MyApp.Test"
    end

    test "test tuple returns module and cleaned test name" do
      result = IssueFormatting.format_scope({:test, MyApp.Test, :"test does stuff"})
      assert result == "MyApp.Test > \"does stuff\""
    end
  end

  # --- timeout_source_label/1 ---

  describe "timeout_source_label/1" do
    test "startup_timeout" do
      assert IssueFormatting.timeout_source_label(:startup_timeout) == "Startup Timeout"
    end

    test "shutdown_timeout" do
      assert IssueFormatting.timeout_source_label(:shutdown_timeout) == "Shutdown Timeout"
    end

    test "test_timeout" do
      assert IssueFormatting.timeout_source_label(:test_timeout) == "Test Timeout"
    end

    test "global_timeout" do
      assert IssueFormatting.timeout_source_label(:global_timeout) == "Global Timeout"
    end

    test "unknown source uses fallback" do
      assert IssueFormatting.timeout_source_label(:something_else) == "Timeout: something_else"
    end
  end

  # --- truncate/2 ---

  describe "truncate/2" do
    test "nil text returns nil" do
      assert IssueFormatting.truncate(nil, 10) == nil
    end

    test "text within limit is unchanged" do
      text = "line1\nline2\nline3"
      assert IssueFormatting.truncate(text, 5) == text
    end

    test "text at exact limit is unchanged" do
      text = "line1\nline2\nline3"
      assert IssueFormatting.truncate(text, 3) == text
    end

    test "text exceeding limit is truncated with ellipsis" do
      text = "a\nb\nc\nd\ne"
      result = IssueFormatting.truncate(text, 3)
      assert result == "a\nb\nc\n... (2 more lines)"
    end

    test "single line text" do
      assert IssueFormatting.truncate("hello", 1) == "hello"
    end

    test "truncate to 1 line from multi-line" do
      result = IssueFormatting.truncate("a\nb\nc", 1)
      assert result == "a\n... (2 more lines)"
    end
  end

  # --- format_pid/1 ---

  describe "format_pid/1" do
    test "nil returns nil" do
      assert IssueFormatting.format_pid(nil) == nil
    end

    test "formats integer pid" do
      assert IssueFormatting.format_pid(1234) == "PID 1234"
    end

    test "formats string pid" do
      assert IssueFormatting.format_pid("1234") == "PID 1234"
    end
  end

  # --- format_signal/1 ---

  describe "format_signal/1" do
    test "nil returns nil" do
      assert IssueFormatting.format_signal(nil) == nil
    end

    test "known signal number formats with name" do
      # SIGSEGV = 11
      result = IssueFormatting.format_signal(11)
      assert result == "signal: SIGSEGV (11)"
    end

    test "SIGABRT = 6" do
      result = IssueFormatting.format_signal(6)
      assert result == "signal: SIGABRT (6)"
    end

    test "SIGKILL = 9" do
      result = IssueFormatting.format_signal(9)
      assert result == "signal: SIGKILL (9)"
    end
  end

  # --- format_exit_status/1 ---

  describe "format_exit_status/1" do
    test "nil returns nil" do
      assert IssueFormatting.format_exit_status(nil) == nil
    end

    test "formats integer status" do
      assert IssueFormatting.format_exit_status(139) == "exit_status: 139"
    end

    test "formats zero status" do
      assert IssueFormatting.format_exit_status(0) == "exit_status: 0"
    end
  end

  # --- format_timestamp/1 ---

  describe "format_timestamp/1" do
    test "DateTime formats to ISO 8601" do
      ts = ~U[2026-03-09 10:30:00Z]
      assert IssueFormatting.format_timestamp(ts) == "at: 2026-03-09T10:30:00Z"
    end

    test "microsecond integer converts then formats" do
      ts = ~U[2026-03-09 10:30:00Z]
      us = DateTime.to_unix(ts, :microsecond)
      assert IssueFormatting.format_timestamp(us) == "at: 2026-03-09T10:30:00.000000Z"
    end

    test "nil returns nil" do
      assert IssueFormatting.format_timestamp(nil) == nil
    end

    test "non-timestamp value returns nil" do
      assert IssueFormatting.format_timestamp("not a timestamp") == nil
    end
  end

  # --- format_coredump_backtrace/1 ---

  describe "format_coredump_backtrace/1" do
    test "formats backtrace from first thread" do
      coredump = %{
        threads: [
          %{frames: [%{function: "crash_here", file: "crash.cpp", line: 42}]}
        ]
      }

      result = IssueFormatting.format_coredump_backtrace(coredump)
      assert result == "#0 crash_here at crash.cpp:42"
    end

    test "truncates beyond 20 frames with ellipsis" do
      frames = for i <- 1..25, do: %{function: "fn_#{i}", file: "f.cpp", line: i}

      coredump = %{threads: [%{frames: frames}]}
      result = IssueFormatting.format_coredump_backtrace(coredump)
      assert result =~ "#0 fn_1"
      assert result =~ "#19 fn_20"
      refute result =~ "#20 fn_21"
      assert String.ends_with?(result, "\n...")
    end

    test "no truncation suffix when frames within limit" do
      frames = [%{function: "main", file: "main.cpp", line: 1}]
      coredump = %{threads: [%{frames: frames}]}
      result = IssueFormatting.format_coredump_backtrace(coredump)
      refute result =~ "..."
    end

    test "returns nil for empty threads" do
      assert IssueFormatting.format_coredump_backtrace(%{threads: []}) == nil
    end

    test "returns nil for no threads key" do
      assert IssueFormatting.format_coredump_backtrace(%{}) == nil
    end

    test "handles thread with no frames key" do
      coredump = %{threads: [%{}]}
      result = IssueFormatting.format_coredump_backtrace(coredump)
      assert result == ""
    end
  end

  # --- format_coredump_path/1 ---

  describe "format_coredump_path/1" do
    test "direct core_path" do
      assert IssueFormatting.format_coredump_path(%{core_path: "/tmp/core.1"}) ==
               "Coredump: /tmp/core.1"
    end

    test "resolves through coredumps list" do
      detail = %{coredumps: [%{core_path: "/tmp/core.2"}]}
      assert IssueFormatting.format_coredump_path(detail) == "Coredump: /tmp/core.2"
    end

    test "returns nil for empty coredumps list" do
      assert IssueFormatting.format_coredump_path(%{coredumps: []}) == nil
    end

    test "returns nil for no relevant keys" do
      assert IssueFormatting.format_coredump_path(%{}) == nil
    end

    test "non-binary core_path falls through to coredumps" do
      detail = %{core_path: nil, coredumps: [%{core_path: "/tmp/core.3"}]}
      assert IssueFormatting.format_coredump_path(detail) == "Coredump: /tmp/core.3"
    end
  end

  # --- format_log_path/1 ---

  describe "format_log_path/1" do
    test "formats log file path" do
      assert IssueFormatting.format_log_path(%{log_file: "/var/log/arangod.log"}) ==
               "Log: /var/log/arangod.log"
    end

    test "returns nil when no log_file key" do
      assert IssueFormatting.format_log_path(%{}) == nil
    end

    test "returns nil for nil log_file" do
      assert IssueFormatting.format_log_path(%{log_file: nil}) == nil
    end
  end
end
