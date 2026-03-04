defmodule ToastTest.ResultExporter.JSONTest do
  use ExUnit.Case, async: true

  alias ToastTest.ResultExporter.JSON
  alias Toast.Deployment.ServerInstance

  @started_at ~U[2024-01-15 10:00:00Z]
  @finished_at ~U[2024-01-15 10:02:00Z]

  defp base_test_results do
    %{
      started_at: @started_at,
      finished_at: @finished_at,
      times_us: %{async: 0, load: 1_000_000, run: 120_000_000},
      modules: %{
        SomeTest => %{
          tests: [
            %{
              module: SomeTest,
              name: "test passes",
              outcome: :passed,
              duration_us: 25_000,
              failure: nil,
              tags: %{file: "test/some_test.exs", line: 5}
            },
            %{
              module: SomeTest,
              name: "test fails",
              outcome: :failed,
              duration_us: 100_000,
              failure: [
                %{
                  kind: "ExUnit.AssertionError",
                  message: "Expected true, got false",
                  stacktrace: "test/some_test.exs:11"
                }
              ],
              tags: %{file: "test/some_test.exs", line: 10}
            }
          ],
          started_at: @started_at,
          finished_at: @finished_at
        },
        OtherTest => %{
          tests: [
            %{
              module: OtherTest,
              name: "test skipped",
              outcome: :skipped,
              duration_us: 0,
              failure: %{message: "not implemented"},
              tags: %{file: "test/other_test.exs", line: 3}
            }
          ],
          started_at: @started_at,
          finished_at: @finished_at
        }
      }
    }
  end

  defp server_diag_entry do
    %{
      sanitizer_errors: [
        %{
          content: "ERROR: AddressSanitizer: heap-buffer-overflow",
          file_path: "/tmp/alubsan.log.arangod.123",
          timestamp: ~U[2024-01-15 10:01:30Z],
          sanitizer_type: :alubsan,
          server_id: "toast-1"
        }
      ],
      log_report: %{
        signal_number: 11,
        signal_name: "SIGSEGV",
        crash_header: "caught unexpected signal 11",
        backtrace: ["frame 0 at 0xdead"],
        fatal_lines: ["FATAL error"],
        crash_output: ["caught unexpected signal 11", "frame 0 at 0xdead"],
        assertion_failures: [
          %{timestamp: nil, message: "assertion failed at foo.cpp:42"}
        ],
        warnings: [
          %{timestamp: nil, message: "FATAL shutdown"}
        ]
      },
      server: %ServerInstance{
        id: "toast-1",
        role: :single,
        pid: 12_345,
        endpoint: "http://127.0.0.1:8529",
        log_file: "/tmp/toast/server/log"
      }
    }
  end

  defp single_server_diagnostics do
    %{"toast-1" => server_diag_entry()}
  end

  defp cluster_diagnostics do
    %{
      "agent-1" => %{
        sanitizer_errors: [],
        log_report: %{
          signal_number: nil,
          signal_name: nil,
          crash_header: nil,
          backtrace: [],
          fatal_lines: [],
          crash_output: [],
          assertion_failures: [],
          warnings: []
        },
        server: %ServerInstance{
          id: "agent-1",
          role: :agent,
          pid: 10_001,
          endpoint: "http://127.0.0.1:8531",
          log_file: "/tmp/toast/agent-1/log"
        }
      },
      "dbserver-1" => %{
        server_diag_entry()
        | server: %ServerInstance{
            id: "dbserver-1",
            role: :dbserver,
            pid: 10_002,
            endpoint: "http://127.0.0.1:8530",
            log_file: "/tmp/toast/dbserver-1/log"
          }
      }
    }
  end

  describe "render/2" do
    test "produces valid JSON" do
      json_string = JSON.render(base_test_results(), nil)
      decoded = :json.decode(json_string)

      assert is_map(decoded)
      assert Map.has_key?(decoded, "toast_version")
    end
  end

  describe "build/2 summary" do
    test "counts are correct for mixed outcomes" do
      result = JSON.build(base_test_results(), nil)

      assert result["summary"]["total"] == 3
      assert result["summary"]["passed"] == 1
      assert result["summary"]["failed"] == 1
      assert result["summary"]["skipped"] == 1
      assert result["summary"]["excluded"] == 0
      assert result["summary"]["invalid"] == 0
    end

    test "skipped, excluded, and invalid tests counted correctly" do
      results = %{
        base_test_results()
        | modules: %{
            A => %{
              tests: [
                %{
                  module: A,
                  name: "t1",
                  outcome: :skipped,
                  duration_us: 0,
                  failure: nil,
                  tags: %{file: "a.exs", line: 1}
                },
                %{
                  module: A,
                  name: "t2",
                  outcome: :excluded,
                  duration_us: 0,
                  failure: nil,
                  tags: %{file: "a.exs", line: 2}
                },
                %{
                  module: A,
                  name: "t3",
                  outcome: :invalid,
                  duration_us: 0,
                  failure: nil,
                  tags: %{file: "a.exs", line: 3}
                },
                %{
                  module: A,
                  name: "t4",
                  outcome: :passed,
                  duration_us: 1000,
                  failure: nil,
                  tags: %{file: "a.exs", line: 4}
                }
              ],
              started_at: @started_at,
              finished_at: @finished_at
            }
          }
      }

      result = JSON.build(results, nil)

      assert result["summary"]["total"] == 4
      assert result["summary"]["skipped"] == 1
      assert result["summary"]["excluded"] == 1
      assert result["summary"]["invalid"] == 1
      assert result["summary"]["passed"] == 1
    end
  end

  describe "build/2 modules" do
    test "grouped by module name" do
      result = JSON.build(base_test_results(), nil)

      modules = result["modules"]
      assert Map.has_key?(modules, "Elixir.SomeTest")
      assert Map.has_key?(modules, "Elixir.OtherTest")
      assert length(modules["Elixir.SomeTest"]["tests"]) == 2
      assert length(modules["Elixir.OtherTest"]["tests"]) == 1
    end

    test "per-module summary counts are correct" do
      result = JSON.build(base_test_results(), nil)

      some_summary = result["modules"]["Elixir.SomeTest"]["summary"]
      assert some_summary["total"] == 2
      assert some_summary["passed"] == 1
      assert some_summary["failed"] == 1
      assert some_summary["skipped"] == 0
    end
  end

  describe "build/2 test entries" do
    test "failed test includes failure info with kind, message, stacktrace" do
      result = JSON.build(base_test_results(), nil)

      failed =
        result["modules"]["Elixir.SomeTest"]["tests"]
        |> Enum.find(&(&1["outcome"] == "failed"))

      assert is_list(failed["failure"])
      [failure] = failed["failure"]
      assert failure["kind"] == "ExUnit.AssertionError"
      assert failure["message"] == "Expected true, got false"
      assert failure["stacktrace"] == "test/some_test.exs:11"
    end

    test "passed test has null failure" do
      result = JSON.build(base_test_results(), nil)

      passed =
        result["modules"]["Elixir.SomeTest"]["tests"]
        |> Enum.find(&(&1["outcome"] == "passed"))

      assert passed["failure"] == nil
    end
  end

  describe "build/2 diagnostics" do
    test "nil diagnostics produces null server_health" do
      result = JSON.build(base_test_results(), nil)

      assert result["server_health"] == nil
    end

    test "single-server diagnostics rendered in server_health" do
      result = JSON.build(base_test_results(), single_server_diagnostics())

      health = result["server_health"]
      assert is_map(health)
      assert Map.has_key?(health, "toast-1")

      server_health = health["toast-1"]

      [san_error] = server_health["sanitizer_errors"]
      assert san_error["content"] == "ERROR: AddressSanitizer: heap-buffer-overflow"
      assert san_error["sanitizer_type"] == "alubsan"
      assert san_error["timestamp"] == "2024-01-15T10:01:30Z"

      assert server_health["crash_report"]["signal_number"] == 11
      assert server_health["crash_report"]["signal_name"] == "SIGSEGV"
      assert server_health["crash_report"]["backtrace"] == ["frame 0 at 0xdead"]

      assert server_health["log_issues"]["assertion_failures"] == ["assertion failed at foo.cpp:42"]
      assert server_health["log_issues"]["warnings"] == ["FATAL shutdown"]
    end

    test "log_file included in server instance" do
      result = JSON.build(base_test_results(), single_server_diagnostics())
      assert result["server_health"]["toast-1"]["server"]["log_file"] == "/tmp/toast/server/log"
    end

    test "server instance included in server_health" do
      result = JSON.build(base_test_results(), single_server_diagnostics())
      server = result["server_health"]["toast-1"]["server"]
      assert server["id"] == "toast-1"
      assert server["role"] == "single"
      assert server["pid"] == 12_345
      assert server["endpoint"] == "http://127.0.0.1:8529"
    end

    test "cluster diagnostics (per-server map) rendered correctly" do
      result = JSON.build(base_test_results(), cluster_diagnostics())

      health = result["server_health"]
      assert Map.has_key?(health, "agent-1")
      assert Map.has_key?(health, "dbserver-1")

      # agent-1 has no sanitizer errors
      assert health["agent-1"]["sanitizer_errors"] == []

      # dbserver-1 has sanitizer errors
      assert length(health["dbserver-1"]["sanitizer_errors"]) == 1
      assert health["dbserver-1"]["crash_report"]["signal_number"] == 11
    end
  end

  describe "to_server_entries/1" do
    test "returns empty list for empty map" do
      assert Toast.Diagnostics.to_server_entries(%{}) == []
    end

    test "returns entries for diagnostics with string keys and map values" do
      diagnostics = %{
        "agent-1" => %{
          sanitizer_errors: [],
          log_report: nil
        },
        "dbserver-1" => %{
          sanitizer_errors: [%{content: "error"}],
          log_report: nil
        }
      }

      entries = Toast.Diagnostics.to_server_entries(diagnostics)
      assert length(entries) == 2
      ids = Enum.map(entries, &elem(&1, 0))
      assert "agent-1" in ids
      assert "dbserver-1" in ids
    end

    test "filters out non-binary keys" do
      diagnostics = %{
        "server-1" => %{sanitizer_errors: []},
        some_atom_key: "ignored"
      }

      entries = Toast.Diagnostics.to_server_entries(diagnostics)
      assert [{"server-1", _}] = entries
    end

    test "filters out non-map values" do
      diagnostics = %{
        "server-1" => %{sanitizer_errors: []},
        "not-a-server" => "string value"
      }

      entries = Toast.Diagnostics.to_server_entries(diagnostics)
      assert [{"server-1", _}] = entries
    end
  end

  describe "build/2 duration" do
    test "calculated correctly from microseconds to seconds" do
      result = JSON.build(base_test_results(), nil)

      assert result["test_run"]["duration_seconds"] == 120.0

      passed =
        result["modules"]["Elixir.SomeTest"]["tests"]
        |> Enum.find(&(&1["outcome"] == "passed"))

      assert passed["duration_seconds"] == 0.025
    end
  end

  describe "build/4 crash matching" do
    test "includes crash_matching when present" do
      crash_matching = %{
        matched: [
          %{
            module: SomeTest,
            test: "test passes",
            confidence: :high,
            crash: %{
              server_id: "toast-1",
              signal_name: "SIGSEGV",
              signal_number: 11,
              crash_header: "caught unexpected signal 11",
              backtrace: ["frame 0 at 0xdead"],
              fatal_lines: [],
              crash_output: ["caught unexpected signal 11"],
              log_file: "/tmp/toast/server/log",
              timestamp: ~U[2024-01-15 10:01:30Z]
            }
          }
        ],
        unmatched: []
      }

      result = JSON.build(base_test_results(), nil, nil, crash_matching)

      assert Map.has_key?(result, "crash_matching")
      assert [entry] = result["crash_matching"]["matched"]
      assert entry["module"] == "Elixir.SomeTest"
      assert entry["test"] == "test passes"
      assert entry["confidence"] == "high"
      assert entry["crash"]["signal_name"] == "SIGSEGV"
      assert entry["crash"]["server_id"] == "toast-1"
      assert entry["crash"]["timestamp"] == "2024-01-15T10:01:30Z"
    end

    test "omits crash_matching when nil" do
      result = JSON.build(base_test_results(), nil, nil, nil)
      refute Map.has_key?(result, "crash_matching")
    end
  end
end
