defmodule Toast.Diagnostics.SummaryTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.{LogReport, Summary}
  alias Toast.Deployment.ServerInstance

  defp base_diag_entry(overrides) do
    Map.merge(
      %{
        sanitizer_errors: [],
        log_report: %LogReport{},
        server_error: nil,
        server: %ServerInstance{
          id: "toast-42",
          role: :single,
          pid: 12_345,
          endpoint: "http://127.0.0.1:8529",
          log_file: "/tmp/toast/server/log"
        }
      },
      overrides
    )
  end

  defp base_diagnostics(overrides \\ %{}) do
    entry = base_diag_entry(overrides)
    server_id = entry.server.id
    %{server_id => entry}
  end

  defp crashed_diag_entry(overrides) do
    base_diag_entry(
      Map.merge(
        %{
          log_report: %LogReport{
            signal_number: 11,
            signal_name: "SIGSEGV",
            crash_header: "caught unexpected signal 11 (SIGSEGV)",
            backtrace: ["frame 0 at 0xdead", "frame 1 at 0xbeef"],
            fatal_lines: ["FATAL: something went wrong"],
            crash_output: [
              "caught unexpected signal 11 (SIGSEGV)",
              "physical memory: 16384, rss usage: 1234567",
              "frame 0 at 0xdead",
              "frame 1 at 0xbeef"
            ]
          },
          server_error:
            {:server_crashed,
             %Toast.Process.CrashInfo{
               exit_status: 139,
               signal: 11,
               timestamp: ~U[2024-01-15 10:01:30Z]
             }}
        },
        overrides
      )
    )
  end

  defp crashed_diagnostics(overrides \\ %{}) do
    entry = crashed_diag_entry(overrides)
    server_id = entry.server.id
    %{server_id => entry}
  end

  describe "format_crashed_servers/1" do
    test "returns nil for nil input" do
      assert Summary.format_crashed_servers(nil) == nil
    end

    test "returns nil for single-server diagnostics with no crash" do
      diag = base_diagnostics()
      assert Summary.format_crashed_servers(diag) == nil
    end

    test "formats single-server crash with signal, crash output, and log path" do
      diag = crashed_diagnostics()
      text = Summary.format_crashed_servers(diag)

      assert text =~ "CRASHED SERVERS"
      assert text =~ "toast-42 (PID 12345, http://127.0.0.1:8529):"
      assert text =~ "Signal: SIGSEGV (signal 11)"
      assert text =~ "Crash output:"
      assert text =~ "caught unexpected signal 11 (SIGSEGV)"
      assert text =~ "physical memory: 16384"
      assert text =~ "frame 0 at 0xdead"
      assert text =~ "frame 1 at 0xbeef"
      assert text =~ "Fatal log entries:"
      assert text =~ "FATAL: something went wrong"
      assert text =~ "Log: /tmp/toast/server/log"
    end

    test "cluster with one crashed server shows only the crashed server" do
      cluster_diag = %{
        "agent-1" =>
          base_diag_entry(%{
            server: %ServerInstance{
              id: "agent-1",
              role: :agent,
              pid: 10_001,
              endpoint: "http://127.0.0.1:8531",
              log_file: "/tmp/toast/agent-1/log"
            }
          }),
        "dbserver-1" =>
          crashed_diag_entry(%{
            server: %ServerInstance{
              id: "dbserver-1",
              role: :dbserver,
              pid: 10_002,
              endpoint: "http://127.0.0.1:8530",
              log_file: "/tmp/toast/dbserver-1/log"
            }
          })
      }

      text = Summary.format_crashed_servers(cluster_diag)

      assert text =~ "CRASHED SERVERS"
      assert text =~ "dbserver-1 (PID 10002, http://127.0.0.1:8530):"
      assert text =~ "Signal: SIGSEGV"
      assert text =~ "Log: /tmp/toast/dbserver-1/log"
      refute text =~ "agent-1"
    end

    test "shows server header without PID/endpoint when not available" do
      diag =
        crashed_diagnostics(%{
          server: %ServerInstance{id: "toast-99", role: :single, pid: nil, endpoint: nil}
        })

      text = Summary.format_crashed_servers(diag)

      assert text =~ "  toast-99:\n"
      refute text =~ "PID"
    end

    test "detects crash from server_error {:server_crashed, _} without signal_name" do
      diag =
        base_diagnostics(%{
          server_error:
            {:server_crashed,
             %Toast.Process.CrashInfo{
               exit_status: 139,
               signal: 11,
               timestamp: ~U[2024-01-15 10:01:30Z]
             }}
        })

      text = Summary.format_crashed_servers(diag)

      assert text =~ "CRASHED SERVERS"
      assert text =~ "Exit: signal 11, exit_status 139"
      assert text =~ "No crash information found in server log."
    end

    test "detects crash from server_error {:server_unhealthy, _}" do
      diag = base_diagnostics(%{server_error: {:server_unhealthy, "server-1"}})
      text = Summary.format_crashed_servers(diag)

      assert text =~ "CRASHED SERVERS"
      assert text =~ "Server became unresponsive"
    end

    test "shows 'no crash information' when crash_output is empty" do
      diag =
        crashed_diagnostics(%{
          log_report: %LogReport{
            signal_number: 11,
            signal_name: "SIGSEGV",
            crash_header: "caught unexpected signal 11 (SIGSEGV)"
          }
        })

      text = Summary.format_crashed_servers(diag)

      assert text =~ "CRASHED SERVERS"
      assert text =~ "Signal: SIGSEGV"
      refute text =~ "Crash output:"
      assert text =~ "No crash information found in server log."
    end

    test "omits Fatal log entries section when fatal_lines is empty" do
      diag =
        crashed_diagnostics(%{
          log_report: %LogReport{
            signal_number: 11,
            signal_name: "SIGSEGV",
            crash_header: "caught unexpected signal 11 (SIGSEGV)",
            crash_output: ["caught unexpected signal 11 (SIGSEGV)"]
          }
        })

      text = Summary.format_crashed_servers(diag)

      assert text =~ "CRASHED SERVERS"
      assert text =~ "Crash output"
      refute text =~ "Fatal log entries"
    end
  end

  describe "format_sanitizer_issues/1" do
    defp make_sanitizer_error(opts \\ %{}) do
      Map.merge(
        %{
          content: "==12345==ERROR: AddressSanitizer: heap-buffer-overflow\nREAD of size 8\n",
          file_path: "/tmp/toast/server/alubsan.log.arangod.12345",
          timestamp: ~U[2024-06-15 10:00:05Z],
          sanitizer_type: :alubsan,
          server_id: "toast-42"
        },
        opts
      )
    end

    test "returns nil when matched and unmatched are both empty" do
      assert Summary.format_sanitizer_issues(%{matched: [], unmatched: []}) == nil
    end

    test "returns nil for non-matching input" do
      assert Summary.format_sanitizer_issues(nil) == nil
      assert Summary.format_sanitizer_issues(%{}) == nil
    end

    test "formats matched sanitizer issues with test attribution" do
      match_result = %{
        matched: [
          %{
            module: SmokeTest.VersionTest,
            test: "test server version",
            confidence: :high,
            error: make_sanitizer_error()
          }
        ],
        unmatched: []
      }

      text = Summary.format_sanitizer_issues(match_result)

      assert text =~ "SANITIZER ISSUES"
      assert text =~ "SmokeTest.VersionTest"
      assert text =~ "test server version"
      assert text =~ "high confidence"
      assert text =~ "[ALUBSAN]"
      assert text =~ "toast-42"
      assert text =~ "AddressSanitizer"
    end

    test "formats unmatched sanitizer issues" do
      match_result = %{
        matched: [],
        unmatched: [make_sanitizer_error()]
      }

      text = Summary.format_sanitizer_issues(match_result)

      assert text =~ "SANITIZER ISSUES"
      assert text =~ "Not attributed to a specific test"
      assert text =~ "[ALUBSAN]"
      assert text =~ "toast-42"
    end

    test "shows low confidence label" do
      match_result = %{
        matched: [
          %{
            module: SmokeTest.AqlTest,
            test: "test basic AQL",
            confidence: :low,
            error: make_sanitizer_error()
          }
        ],
        unmatched: []
      }

      text = Summary.format_sanitizer_issues(match_result)

      assert text =~ "low confidence"
    end

    test "formats TSAN sanitizer type" do
      match_result = %{
        matched: [],
        unmatched: [
          make_sanitizer_error(%{sanitizer_type: :tsan, file_path: "/tmp/tsan.log.arangod.999"})
        ]
      }

      text = Summary.format_sanitizer_issues(match_result)

      assert text =~ "[TSAN]"
    end

    test "truncates long content" do
      long_content = Enum.map_join(1..20, "\n", &"line #{&1} of sanitizer output")

      match_result = %{
        matched: [],
        unmatched: [make_sanitizer_error(%{content: long_content})]
      }

      text = Summary.format_sanitizer_issues(match_result)

      assert text =~ "line 1 of sanitizer output"
      assert text =~ "line 10 of sanitizer output"
      assert text =~ "more lines"
      refute text =~ "line 20 of sanitizer output"
    end

    test "includes file path reference" do
      match_result = %{
        matched: [],
        unmatched: [
          make_sanitizer_error(%{file_path: "/tmp/toast/run_1/server/alubsan.log.arangod.12345"})
        ]
      }

      text = Summary.format_sanitizer_issues(match_result)

      assert text =~ "see /tmp/toast/run_1/server/alubsan.log.arangod.12345"
    end

    test "groups multiple errors for same test" do
      match_result = %{
        matched: [
          %{
            module: SmokeTest.VersionTest,
            test: "test server version",
            confidence: :high,
            error: make_sanitizer_error(%{sanitizer_type: :alubsan})
          },
          %{
            module: SmokeTest.VersionTest,
            test: "test server version",
            confidence: :high,
            error:
              make_sanitizer_error(%{
                sanitizer_type: :tsan,
                file_path: "/tmp/tsan.log.arangod.12345"
              })
          }
        ],
        unmatched: []
      }

      text = Summary.format_sanitizer_issues(match_result)

      assert text =~ "[ALUBSAN]"
      assert text =~ "[TSAN]"
      # The test name should appear only once as a header
      [_ | rest] = String.split(text, "test server version")
      assert length(rest) == 1
    end
  end

  describe "format_crash_attribution/1" do
    defp make_crash_info(opts \\ %{}) do
      Map.merge(
        %{
          server_id: "toast-42",
          signal_name: "SIGSEGV",
          signal_number: 11,
          crash_header: "caught unexpected signal 11 (SIGSEGV)",
          backtrace: ["frame 0 at 0xdead", "frame 1 at 0xbeef"],
          fatal_lines: [],
          crash_output: [
            "caught unexpected signal 11 (SIGSEGV)",
            "frame 0 at 0xdead",
            "frame 1 at 0xbeef"
          ],
          log_file: "/tmp/toast/server/log",
          timestamp: ~U[2024-06-15 10:00:05Z]
        },
        opts
      )
    end

    test "returns nil when matched and unmatched are both empty" do
      assert Summary.format_crash_attribution(%{matched: [], unmatched: []}, []) == nil
    end

    test "returns nil for non-matching input" do
      assert Summary.format_crash_attribution(nil, []) == nil
      assert Summary.format_crash_attribution(%{}, []) == nil
    end

    test "formats matched crash with test attribution" do
      match_result = %{
        matched: [
          %{
            module: SmokeTest.VersionTest,
            test: "test server version",
            confidence: :high,
            crash: make_crash_info()
          }
        ],
        unmatched: []
      }

      text = Summary.format_crash_attribution(match_result, [])

      assert text =~ "CRASH ATTRIBUTION"
      assert text =~ "SmokeTest.VersionTest"
      assert text =~ "test server version"
      assert text =~ "high confidence"
      assert text =~ "SIGSEGV"
      assert text =~ "toast-42"
      assert text =~ "caught unexpected signal 11 (SIGSEGV)"
      assert text =~ "/tmp/toast/server/log"
    end

    test "formats unmatched crashes" do
      match_result = %{
        matched: [],
        unmatched: [make_crash_info()]
      }

      text = Summary.format_crash_attribution(match_result, [])

      assert text =~ "Not attributed to a specific test"
      assert text =~ "SIGSEGV"
    end

    test "shows low confidence label" do
      match_result = %{
        matched: [
          %{
            module: SmokeTest.AqlTest,
            test: "test basic AQL",
            confidence: :low,
            crash: make_crash_info()
          }
        ],
        unmatched: []
      }

      text = Summary.format_crash_attribution(match_result, [])

      assert text =~ "low confidence"
    end
  end
end
