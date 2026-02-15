defmodule Toast.ResultExporter.JUnitXMLTest do
  use ExUnit.Case, async: true

  alias Toast.ResultExporter.JUnitXML

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

  describe "well-formed XML" do
    test "starts with XML declaration and has testsuites root" do
      xml = JUnitXML.render(base_test_results(), nil)

      assert String.starts_with?(xml, ~s(<?xml version="1.0" encoding="UTF-8"?>))
      assert xml =~ ~r/<testsuites\s/
      assert String.ends_with?(xml, "</testsuites>")
    end
  end

  describe "passed test" do
    test "renders as self-closing testcase" do
      xml = JUnitXML.render(base_test_results(), nil)

      assert xml =~ ~r/<testcase name="test passes" classname="Elixir\.SomeTest" time="0\.025"\/>/
    end
  end

  describe "failed test" do
    test "renders with failure child element" do
      xml = JUnitXML.render(base_test_results(), nil)

      assert xml =~ ~r/<testcase name="test fails".*?>/
      assert xml =~ ~r/<failure message="Expected true, got false" type="ExUnit\.AssertionError">/
      assert xml =~ "test/some_test.exs:11</failure>"
    end
  end

  describe "skipped test" do
    test "renders with skipped child element" do
      xml = JUnitXML.render(base_test_results(), nil)

      assert xml =~ ~r/<skipped message="not implemented"\/>/
    end
  end

  describe "invalid test" do
    test "renders with error child element" do
      results = %{
        base_test_results()
        | tests: [
            %{
              module: BrokenTest,
              name: "test broken",
              outcome: :invalid,
              duration_us: 0,
              failure: nil,
              tags: %{file: "test/broken_test.exs", line: 1}
            }
          ]
      }

      xml = JUnitXML.render(results, nil)

      assert xml =~ ~r/<error message="setup_all failed" type="RuntimeError"\/>/
    end
  end

  describe "aggregate counts" do
    test "top-level testsuites has correct counts" do
      xml = JUnitXML.render(base_test_results(), nil)

      assert xml =~ ~s(tests="3")
      assert xml =~ ~s(failures="1")
      assert xml =~ ~s(errors="0")
      # 1 skipped
      assert xml =~ ~r/<testsuites[^>]*skipped="1"/
    end

    test "per-module testsuite has correct counts" do
      xml = JUnitXML.render(base_test_results(), nil)

      # SomeTest: 2 tests, 1 failure, 0 errors, 0 skipped
      assert xml =~ ~r/<testsuite name="Elixir\.SomeTest" tests="2" failures="1" errors="0" skipped="0"/

      # OtherTest: 1 test, 0 failures, 0 errors, 1 skipped
      assert xml =~ ~r/<testsuite name="Elixir\.OtherTest" tests="1" failures="0" errors="0" skipped="1"/
    end
  end

  describe "diagnostics" do
    test "server health diagnostics in system-err CDATA" do
      xml = JUnitXML.render(base_test_results(), single_server_diagnostics())

      assert xml =~ "<system-err>"
      assert xml =~ "<![CDATA["
      assert xml =~ "Sanitizer Errors:"
      assert xml =~ "Crash Report:"
      assert xml =~ "Signal: SIGSEGV (11)"
      assert xml =~ "Log Issues:"
    end

    test "no system-err when diagnostics are nil" do
      xml = JUnitXML.render(base_test_results(), nil)

      refute xml =~ "<system-err>"
    end
  end

  describe "XML escaping" do
    test "special characters escaped in test names" do
      results = %{
        base_test_results()
        | tests: [
            %{
              module: SpecialTest,
              name: ~s(test with <special> & "chars"),
              outcome: :passed,
              duration_us: 1000,
              failure: nil,
              tags: %{file: "test/special_test.exs", line: 1}
            }
          ]
      }

      xml = JUnitXML.render(results, nil)

      assert xml =~ "&lt;special&gt;"
      assert xml =~ "&amp;"
      assert xml =~ "&quot;chars&quot;"
      refute xml =~ ~s(name="test with <special>)
    end
  end

  describe "duration formatting" do
    test "formatted with 3 decimal places" do
      xml = JUnitXML.render(base_test_results(), nil)

      # 120_000_000 us = 120.000 seconds
      assert xml =~ ~r/<testsuites[^>]*time="120\.000"/

      # 25_000 us = 0.025 seconds
      assert xml =~ ~s(time="0.025")

      # 100_000 us = 0.100 seconds
      assert xml =~ ~s(time="0.100")
    end
  end
end
