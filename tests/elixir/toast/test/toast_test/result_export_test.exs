defmodule ToastTest.ResultExportTest do
  use ExUnit.Case, async: true

  alias ToastTest.ResultExporter.{JSON, JUnitXML}

  defp sample_suite_results do
    %{
      started_at: ~U[2026-01-01 10:00:00Z],
      finished_at: ~U[2026-01-01 10:05:00Z],
      global_duration_us: 300_000_000,
      suites: [
        %{
          name: "smoke",
          suite_module: Smoke.Suite,
          deployment_mode: :single_server,
          started_at: ~U[2026-01-01 10:00:00Z],
          finished_at: ~U[2026-01-01 10:02:00Z],
          duration_us: 120_000_000,
          diagnostics: nil,
          tests: [
            %{module: Smoke.VersionTest, name: "returns version", outcome: :passed, duration_us: 50_000},
            %{module: Smoke.VersionTest, name: "returns server name", outcome: :passed, duration_us: 30_000}
          ]
        },
        %{
          name: "shell_server",
          suite_module: ShellServer.Suite,
          deployment_mode: :single_server,
          started_at: ~U[2026-01-01 10:02:00Z],
          finished_at: ~U[2026-01-01 10:05:00Z],
          duration_us: 180_000_000,
          diagnostics: nil,
          tests: [
            %{module: ShellServer.BasicTest, name: "evaluates expression", outcome: :passed, duration_us: 100_000},
            %{module: ShellServer.BasicTest, name: "handles error", outcome: :failed, duration_us: 200_000}
          ]
        }
      ],
      summary: %{
        total: 4,
        passed: 3,
        failed: 1,
        skipped: 0,
        errored: 0
      }
    }
  end

  describe "JSON suite-level export" do
    test "render_suites produces valid JSON with suite grouping" do
      json_str = JSON.render_suites(sample_suite_results())
      assert is_binary(json_str)
      assert json_str =~ "smoke"
      assert json_str =~ "shell_server"
      assert json_str =~ "returns version"
      assert json_str =~ "evaluates expression"
    end

    test "JSON includes global summary" do
      json_str = JSON.render_suites(sample_suite_results())
      assert json_str =~ "\"total\": 4"
      assert json_str =~ "\"passed\": 3"
      assert json_str =~ "\"failed\": 1"
    end

    test "JSON includes suite-level metadata" do
      json_str = JSON.render_suites(sample_suite_results())
      assert json_str =~ "single_server"
      assert json_str =~ "Elixir.Smoke.Suite"
    end
  end

  describe "JUnit XML suite-level export" do
    test "render_suites produces XML with testsuite elements per suite" do
      xml = JUnitXML.render_suites(sample_suite_results())
      assert xml =~ ~s(<?xml version="1.0")
      assert xml =~ ~s(<testsuites name="toast")
      assert xml =~ ~s(<testsuite name="smoke")
      assert xml =~ ~s(<testsuite name="shell_server")
    end

    test "XML includes test cases within suites" do
      xml = JUnitXML.render_suites(sample_suite_results())
      assert xml =~ ~s(name="returns version")
      assert xml =~ ~s(name="evaluates expression")
    end

    test "XML counts are correct" do
      xml = JUnitXML.render_suites(sample_suite_results())
      assert xml =~ ~s(tests="4")
      assert xml =~ ~s(failures="1")
    end
  end
end
