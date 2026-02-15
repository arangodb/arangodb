defmodule Toast.ResultExporter.JSONTest do
  use ExUnit.Case, async: true

  alias Toast.ResultExporter.JSON

  @started_at ~U[2024-01-15 10:00:00Z]
  @finished_at ~U[2024-01-15 10:02:00Z]

  defp base_test_results do
    %{
      suite_started_at: @started_at,
      suite_finished_at: @finished_at,
      times_us: %{async: 0, load: 1_000_000, run: 120_000_000},
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
        },
        %{
          module: OtherTest,
          name: "test skipped",
          outcome: :skipped,
          duration_us: 0,
          failure: %{message: "not implemented"},
          tags: %{file: "test/other_test.exs", line: 3}
        }
      ]
    }
  end

  defp single_server_diagnostics do
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
      server_log: %{assertion_failures: ["assertion failed at foo.cpp:42"], warnings: ["FATAL shutdown"]},
      crash_report: %{
        signal_number: 11,
        signal_name: "SIGSEGV",
        crash_header: "caught unexpected signal 11",
        backtrace: ["frame 0 at 0xdead"],
        fatal_lines: ["FATAL error"]
      }
    }
  end

  defp cluster_diagnostics do
    %{
      "agent-1" => %{
        sanitizer_errors: [],
        server_log: %{assertion_failures: [], warnings: []},
        crash_report: %{
          signal_number: nil,
          signal_name: nil,
          crash_header: nil,
          backtrace: [],
          fatal_lines: []
        }
      },
      "dbserver-1" => single_server_diagnostics()
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
        | tests: [
            %{module: A, name: "t1", outcome: :skipped, duration_us: 0, failure: nil, tags: %{file: "a.exs", line: 1}},
            %{module: A, name: "t2", outcome: :excluded, duration_us: 0, failure: nil, tags: %{file: "a.exs", line: 2}},
            %{module: A, name: "t3", outcome: :invalid, duration_us: 0, failure: nil, tags: %{file: "a.exs", line: 3}},
            %{module: A, name: "t4", outcome: :passed, duration_us: 1000, failure: nil, tags: %{file: "a.exs", line: 4}}
          ]
      }

      result = JSON.build(results, nil)

      assert result["summary"]["total"] == 4
      assert result["summary"]["skipped"] == 1
      assert result["summary"]["excluded"] == 1
      assert result["summary"]["invalid"] == 1
      assert result["summary"]["passed"] == 1
    end
  end

  describe "build/2 test suites" do
    test "grouped by module name" do
      result = JSON.build(base_test_results(), nil)

      suites = result["test_suites"]
      assert Map.has_key?(suites, "Elixir.SomeTest")
      assert Map.has_key?(suites, "Elixir.OtherTest")
      assert length(suites["Elixir.SomeTest"]["tests"]) == 2
      assert length(suites["Elixir.OtherTest"]["tests"]) == 1
    end

    test "per-suite summary counts are correct" do
      result = JSON.build(base_test_results(), nil)

      some_summary = result["test_suites"]["Elixir.SomeTest"]["summary"]
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
        result["test_suites"]["Elixir.SomeTest"]["tests"]
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
        result["test_suites"]["Elixir.SomeTest"]["tests"]
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

      [san_error] = health["sanitizer_errors"]
      assert san_error["content"] == "ERROR: AddressSanitizer: heap-buffer-overflow"
      assert san_error["sanitizer_type"] == "alubsan"
      assert san_error["timestamp"] == "2024-01-15T10:01:30Z"

      assert health["crash_report"]["signal_number"] == 11
      assert health["crash_report"]["signal_name"] == "SIGSEGV"
      assert health["crash_report"]["backtrace"] == ["frame 0 at 0xdead"]

      assert health["log_issues"]["assertion_failures"] == ["assertion failed at foo.cpp:42"]
      assert health["log_issues"]["warnings"] == ["FATAL shutdown"]
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

  describe "cluster_diagnostics?/1" do
    test "returns false for empty map" do
      refute Toast.ResultExporter.cluster_diagnostics?(%{})
    end

    test "returns false for single-server diagnostics with atom keys" do
      diagnostics = %{
        sanitizer_errors: [],
        server_log: %{assertion_failures: [], warnings: []},
        crash_report: nil
      }

      refute Toast.ResultExporter.cluster_diagnostics?(diagnostics)
    end

    test "returns true for cluster diagnostics with string keys and :sanitizer_errors in values" do
      diagnostics = %{
        "agent-1" => %{
          sanitizer_errors: [],
          server_log: nil,
          crash_report: nil
        },
        "dbserver-1" => %{
          sanitizer_errors: [%{content: "error"}],
          server_log: nil,
          crash_report: nil
        }
      }

      assert Toast.ResultExporter.cluster_diagnostics?(diagnostics)
    end

    test "returns false for map with string keys but values without :sanitizer_errors" do
      diagnostics = %{
        "server-1" => %{some_other_key: "value"},
        "server-2" => %{another_key: "value"}
      }

      refute Toast.ResultExporter.cluster_diagnostics?(diagnostics)
    end
  end

  describe "build/2 duration" do
    test "calculated correctly from microseconds to seconds" do
      result = JSON.build(base_test_results(), nil)

      assert result["test_run"]["duration_seconds"] == 120.0

      passed =
        result["test_suites"]["Elixir.SomeTest"]["tests"]
        |> Enum.find(&(&1["outcome"] == "passed"))

      assert passed["duration_seconds"] == 0.025
    end
  end
end
