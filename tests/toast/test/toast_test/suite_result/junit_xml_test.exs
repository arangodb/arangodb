defmodule ToastTest.SuiteResult.JUnitXMLTest do
  use ExUnit.Case, async: true

  alias ToastTest.SuiteResult

  import ToastTest.SuiteResultTestHelpers

  defp read_xml!(dir, filename) do
    dir
    |> Path.join(filename)
    |> File.read!()
  end

  # --- Tests ---

  test "creates an XML file at the expected path" do
    with_tmp_dir(fn dir ->
      result = build_suite_result()
      SuiteResult.write_junit_xml(result, dir)

      assert File.exists?(Path.join(dir, "smoke.xml"))
    end)
  end

  test "output starts with XML declaration" do
    with_tmp_dir(fn dir ->
      result = build_suite_result()
      SuiteResult.write_junit_xml(result, dir)

      content = File.read!(Path.join(dir, "smoke.xml"))
      assert String.starts_with?(content, ~s(<?xml version="1.0"))
    end)
  end

  test "contains testsuites root element" do
    with_tmp_dir(fn dir ->
      result = build_suite_result()
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testsuites\s/
      assert xml =~ ~r/<\/testsuites>/
    end)
  end

  test "contains testsuite element per module" do
    with_tmp_dir(fn dir ->
      result = build_suite_result()
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testsuite\s[^>]*name="Elixir.FakeModule"/
    end)
  end

  test "contains testcase elements for each test" do
    with_tmp_dir(fn dir ->
      result = build_suite_result()
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testcase\s[^>]*name="test passes"/
      assert xml =~ ~r/<testcase\s[^>]*name="test fails"/
    end)
  end

  test "includes correct test and failure counts in testsuites" do
    with_tmp_dir(fn dir ->
      result = build_suite_result()
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      # 2 real tests + 1 synthetic for module-scoped crash
      assert xml =~ ~r/<testsuites[^>]*tests="3"/
      assert xml =~ ~r/<testsuites[^>]*failures="1"/
    end)
  end

  test "failed tests have failure elements" do
    issues = [
      %{
        type: :test_failure,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{
          test: %ExUnit.Test{
            name: :"test fails",
            module: FakeModule,
            state:
              {:failed, [{:error, %ExUnit.AssertionError{message: "Expected 1, got 2"}, []}]},
            tags: %{file: "test/fake_test.exs", line: 20, test_type: :test},
            time: 59_000_000
          }
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<failure/
    end)
  end

  test "module-scoped crash appears as synthetic testcase in testsuite" do
    with_tmp_dir(fn dir ->
      result = build_suite_result()
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      # Synthetic testcase inside the module's testsuite
      assert xml =~
               ~r/<testsuite[^>]*name="Elixir.FakeModule"[^>]*>.*<testcase[^>]*name="crash \(srv-1\)"[^>]*>.*<error message="crash">.*Coredump:.*core\.1234.*<\/error>.*<\/testcase>.*<\/testsuite>/s
    end)
  end

  test "module-scoped issue with setup phase has setup_all prefix in name" do
    issues = [
      %{
        type: :crash,
        scope: {:module, FakeModule},
        confidence: nil,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "setup crash", phase: :setup}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testcase[^>]*name="setup_all crash \(srv-1\)"/
    end)
  end

  test "module-scoped issue with teardown phase has on_exit prefix in name" do
    issues = [
      %{
        type: :crash,
        scope: {:module, FakeModule},
        confidence: nil,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "teardown crash", phase: :teardown}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testcase[^>]*name="on_exit crash \(srv-1\)"/
    end)
  end

  test "suite-scoped sanitizer appears as synthetic testcase in _infrastructure_ testsuite" do
    issues = [build_sanitizer_issue()]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      # Suite-scoped issue in _infrastructure_ testsuite
      assert xml =~
               ~r/<testsuite[^>]*name="_infrastructure_"[^>]*>.*<testcase[^>]*name="sanitizer report \(srv-1\)".*<error message="sanitizer report">.*ASAN detected leak/s
    end)
  end

  test "test-scoped crash appears in testcase error element" do
    issues = [
      %{
        type: :crash,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :high,
        detail: %{
          server: "srv-1",
          coredumps: [%{core_path: "/tmp/core.5678", signal: "SIGSEGV", threads: []}]
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      assert xml =~
               ~r/<testcase[^>]*name="test passes"[^>]*>.*<error message="crash">.*Coredump:.*core\.5678/s
    end)
  end

  test "suite-scoped timeout appears as synthetic testcase in _infrastructure_ testsuite" do
    issues = [
      %{
        type: :timeout,
        scope: :suite,
        confidence: :high,
        detail: %{
          source: "overall",
          reason: "suite exceeded 30 minute limit",
          timestamp: ~U[2026-03-09 10:30:00Z],
          servers: [
            %{server_id: "srv-1", coredump: "/tmp/core.9999"},
            %{server_id: "srv-2", coredump: nil}
          ]
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testsuite[^>]*name="_infrastructure_"/
      assert xml =~ ~r/<testcase[^>]*name="timeout"/
      assert xml =~ "suite exceeded 30 minute limit"
      assert xml =~ "srv-1"
      assert xml =~ "core.9999"
    end)
  end

  test "handles suite with only passed tests and no issues" do
    modules = %{
      CleanModule => %{
        started_at: mod_started_at(),
        finished_at: mod_finished_at(),
        setup_finished_at: nil,
        teardown_started_at: nil,
        tests: [
          %{
            name: :"test ok",
            outcome: :passed,
            duration_us: 1000,
            started_at: test1_started_at(),
            finished_at: test1_finished_at(),
            tags: %{}
          }
        ]
      }
    }

    with_tmp_dir(fn dir ->
      test_data = build_test_data(%{modules: modules})
      result = SuiteResult.build(test_data, [])
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testsuites[^>]*failures="0"/
      refute xml =~ ~r/<failure/
    end)
  end

  test "skipped and excluded tests have skipped elements" do
    modules = %{
      SkipModule => %{
        started_at: mod_started_at(),
        finished_at: mod_finished_at(),
        setup_finished_at: nil,
        teardown_started_at: nil,
        tests: [
          %{
            name: :"test skipped",
            outcome: :skipped,
            duration_us: 0,
            started_at: test1_started_at(),
            finished_at: test1_finished_at(),
            tags: %{}
          },
          %{
            name: :"test excluded",
            outcome: :excluded,
            duration_us: 0,
            started_at: test1_started_at(),
            finished_at: test1_finished_at(),
            tags: %{}
          }
        ]
      }
    }

    with_tmp_dir(fn dir ->
      test_data = build_test_data(%{modules: modules})
      result = SuiteResult.build(test_data, [])
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testsuites[^>]*skipped="2"/
      assert xml =~ ~r/<skipped/
    end)
  end

  test "invalid tests have error elements" do
    modules = %{
      ErrorModule => %{
        started_at: mod_started_at(),
        finished_at: mod_finished_at(),
        setup_finished_at: nil,
        teardown_started_at: nil,
        tests: [
          %{
            name: :"test invalid",
            outcome: :invalid,
            duration_us: 500,
            started_at: test1_started_at(),
            finished_at: test1_finished_at(),
            tags: %{}
          }
        ]
      }
    }

    with_tmp_dir(fn dir ->
      test_data = build_test_data(%{modules: modules})
      result = SuiteResult.build(test_data, [])
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testsuites[^>]*errors="1"/
      assert xml =~ ~r/<testcase[^>]*name="test invalid"[^>]*>\s*<error\/>/s
    end)
  end

  # --- Issue type x scope matrix ---

  test "suite-scoped crash appears as synthetic testcase in _infrastructure_ testsuite" do
    issues = [
      %{
        type: :crash,
        scope: :suite,
        confidence: :medium,
        detail: %{
          server: "srv-1",
          coredumps: [%{core_path: "/tmp/core.100", signal: "SIGABRT", threads: []}],
          crash_lines: "fatal error in main"
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<testsuite[^>]*name="_infrastructure_"/
      assert xml =~ ~r/<testcase[^>]*name="crash \(srv-1\)"/
      assert xml =~ ~r/<error message="crash">/
      assert xml =~ "fatal error in main"
      assert xml =~ "Coredump: /tmp/core.100"
    end)
  end

  test "module-scoped sanitizer appears as synthetic testcase in module testsuite" do
    issues = [
      %{
        type: :sanitizer_report,
        scope: {:module, FakeModule},
        confidence: nil,
        detail: %{server: "srv-2", report: "TSAN data race detected"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      assert xml =~
               ~r/<testsuite[^>]*name="Elixir.FakeModule"[^>]*>.*<testcase[^>]*name="sanitizer report \(srv-2\)"[^>]*>.*<error message="sanitizer report">.*TSAN data race.*<\/error>.*<\/testcase>.*<\/testsuite>/s
    end)
  end

  test "test-scoped sanitizer appears in testcase error element with kind" do
    issues = [
      %{
        type: :sanitizer_report,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :high,
        detail: %{
          server: "srv-1",
          report: "LSAN leak in test_passes",
          kind: "detected memory leaks"
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      assert xml =~
               ~r/<testcase[^>]*name="test passes"[^>]*>\s*<error message="sanitizer report: detected memory leaks">.*LSAN leak/s
    end)
  end

  test "sanitizer error message falls back when kind is nil" do
    issues = [
      %{
        type: :sanitizer_report,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :high,
        detail: %{server: "srv-1", report: "some sanitizer output", kind: nil}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<error message="sanitizer report">/
    end)
  end

  test "module-scoped timeout appears as synthetic testcase in module testsuite" do
    issues = [
      %{
        type: :timeout,
        scope: {:module, FakeModule},
        confidence: :high,
        detail: %{
          source: "module_setup",
          reason: "setup exceeded limit",
          timestamp: ~U[2026-03-09 10:01:00Z],
          servers: [%{server_id: "srv-1", coredump: "/tmp/core.mod"}]
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      assert xml =~
               ~r/<testsuite[^>]*name="Elixir.FakeModule"[^>]*>.*<testcase[^>]*name="timeout"[^>]*>.*<error message="timeout">.*setup exceeded limit.*<\/error>.*<\/testcase>.*<\/testsuite>/s

      assert xml =~ "core.mod"
    end)
  end

  test "test-scoped timeout appears in testcase error element" do
    issues = [
      %{
        type: :timeout,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :high,
        detail: %{
          source: "test_execution",
          reason: "test exceeded 5 minute limit",
          timestamp: ~U[2026-03-09 10:05:00Z],
          servers: []
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      assert xml =~
               ~r/<testcase[^>]*name="test passes"[^>]*>\s*<error message="timeout">.*test exceeded 5 minute/s
    end)
  end

  # --- Detail rendering edge cases ---

  test "crash with crash_lines includes them in output" do
    issues = [
      %{
        type: :crash,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :high,
        detail: %{
          server: "srv-1",
          coredumps: [%{core_path: "/tmp/core.42", signal: "SIGSEGV", threads: []}],
          crash_lines: "thread 1 crashed at 0xdeadbeef\nbacktrace follows"
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ "thread 1 crashed at 0xdeadbeef"
      assert xml =~ "backtrace follows"
    end)
  end

  test "crash with no coredumps renders server and crash_lines only" do
    issues = [
      %{
        type: :crash,
        scope: {:module, FakeModule},
        confidence: :low,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "unexpected shutdown"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ "srv-1 — FakeModule"
      assert xml =~ "unexpected shutdown"
      refute xml =~ "Coredump:"
    end)
  end

  test "crash coredump without signal omits signal" do
    issues = [
      %{
        type: :crash,
        scope: {:module, FakeModule},
        confidence: :low,
        detail: %{
          server: "srv-1",
          coredumps: [%{core_path: "/tmp/core.nosig"}],
          crash_lines: nil
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ "Coredump: /tmp/core.nosig"
      refute xml =~ "Signal:"
    end)
  end

  test "timeout with empty servers list renders source and reason only" do
    issues = [
      %{
        type: :timeout,
        scope: :suite,
        confidence: :high,
        detail: %{
          source: "overall",
          reason: "timed out",
          timestamp: ~U[2026-03-09 10:30:00Z],
          servers: []
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ "[Timeout: overall] timed out"
    end)
  end

  test "timeout with nil detail fields renders available fields only" do
    issues = [
      %{
        type: :timeout,
        scope: :suite,
        confidence: :high,
        detail: %{source: nil, reason: "unknown", servers: nil}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ "[Timeout: ] unknown"
    end)
  end

  # --- Structural edge cases ---

  test "multiple module-scoped issues produce multiple synthetic testcases" do
    issues = [
      %{
        type: :crash,
        scope: {:module, FakeModule},
        confidence: :low,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "crash A"}
      },
      %{
        type: :sanitizer_report,
        scope: {:module, FakeModule},
        confidence: nil,
        detail: %{server: "srv-1", report: "ASAN report B"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      # Each issue gets its own synthetic testcase
      assert xml =~ ~r/<testcase[^>]*name="crash \(srv-1\)"/
      assert xml =~ ~r/<testcase[^>]*name="sanitizer report \(srv-1\)"/
      assert xml =~ "crash A"
      assert xml =~ "ASAN report B"
    end)
  end

  test "multiple test-scoped issues are merged into a single error element" do
    issues = [
      %{
        type: :crash,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :high,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "crash detail"}
      },
      %{
        type: :sanitizer_report,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :high,
        detail: %{server: "srv-1", report: "TSAN report detail", kind: "data race"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      # Only one <error> element in the testcase
      testcase_match =
        Regex.run(
          ~r/<testcase[^>]*name="test passes"[^>]*>(.*?)<\/testcase>/s,
          xml,
          capture: :all_but_first
        )

      assert [inner] = testcase_match
      error_count = length(Regex.scan(~r/<error\b/, inner))
      assert error_count == 1

      # Message lists both issue types
      assert inner =~ ~r/<error message="crash, sanitizer report: data race">/
      # Body contains both details
      assert inner =~ "crash detail"
      assert inner =~ "TSAN report detail"
    end)
  end

  test "issues at all three scope levels appear at correct nesting" do
    issues = [
      %{
        type: :crash,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :high,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "test-level crash"}
      },
      %{
        type: :sanitizer_report,
        scope: {:module, FakeModule},
        confidence: nil,
        detail: %{server: "srv-1", report: "module-level sanitizer"}
      },
      %{
        type: :timeout,
        scope: :suite,
        confidence: :high,
        detail: %{source: "overall", reason: "suite-level timeout", servers: []}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      # Test-level: inside real <testcase> as <error> with detail
      assert xml =~
               ~r/<testcase[^>]*name="test passes"[^>]*>\s*<error message="crash">.*test-level crash/s

      # Module-level: synthetic <testcase> inside <testsuite>
      assert xml =~
               ~r/<testsuite[^>]*name="Elixir.FakeModule"[^>]*>.*<testcase[^>]*name="sanitizer report \(srv-1\)"[^>]*>.*module-level sanitizer.*<\/testsuite>/s

      # Suite-level: synthetic <testcase> in _infrastructure_ testsuite
      assert xml =~
               ~r/<testsuite[^>]*name="_infrastructure_"[^>]*>.*<testcase[^>]*name="timeout"[^>]*>.*suite-level timeout.*<\/testsuite>/s
    end)
  end

  test "passed test with test-scoped crash gets error with detail, without failure" do
    issues = [
      %{
        type: :crash,
        scope: {:test, FakeModule, :"test passes"},
        confidence: :medium,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "post-test crash"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      testcase_match =
        Regex.run(
          ~r/<testcase[^>]*name="test passes"[^>]*>(.*?)<\/testcase>/s,
          xml,
          capture: :all_but_first
        )

      assert [inner] = testcase_match
      assert inner =~ ~r/<error message="crash">.*post-test crash.*<\/error>/s
      refute inner =~ ~r/<failure/
    end)
  end

  test "CDATA content containing ]]> is properly escaped" do
    issues = [
      %{
        type: :sanitizer_report,
        scope: :suite,
        confidence: nil,
        detail: %{server: "srv-1", report: "data]]>end"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      # ]]> in CDATA must be escaped as ]]]]><![CDATA[>
      assert xml =~ "]]]]><![CDATA[>"
      # The raw "data]]>end" must NOT appear verbatim inside CDATA
      refute Regex.match?(~r/<!\[CDATA\[.*data\]\]>end/s, xml)
    end)
  end

  test "unknown issue type produces no synthetic testcase" do
    issues = [
      %{
        type: :unknown_future_type,
        scope: :suite,
        confidence: :low,
        detail: %{something: "irrelevant"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      refute xml =~ ~r/<testsuite[^>]*name="_infrastructure_"/
      refute xml =~ "unknown_future_type"
    end)
  end

  test "xml special characters in test and module names are escaped" do
    modules = %{
      :"Elixir.Module<With>&\"Chars" => %{
        started_at: mod_started_at(),
        finished_at: mod_finished_at(),
        setup_finished_at: nil,
        teardown_started_at: nil,
        tests: [
          %{
            name: :"test <angle> & \"quote\"",
            outcome: :passed,
            duration_us: 1000,
            started_at: test1_started_at(),
            finished_at: test1_finished_at(),
            tags: %{}
          }
        ]
      }
    }

    with_tmp_dir(fn dir ->
      test_data = build_test_data(%{modules: modules})
      result = SuiteResult.build(test_data, [])
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ "&amp;"
      assert xml =~ "&lt;"
      assert xml =~ "&gt;"
      assert xml =~ "&quot;"
    end)
  end

  test "failure without ExUnit.Test detail renders bare failure element" do
    issues = [
      %{
        type: :test_failure,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{test: %{name: :"test fails", module: FakeModule}}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<failure\/>/
    end)
  end

  test "failure with non-assertion error includes formatted details" do
    issues = [
      %{
        type: :test_failure,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{
          test: %ExUnit.Test{
            name: :"test fails",
            module: FakeModule,
            state: {:failed, [{:exit, :timeout, []}]},
            tags: %{file: "test/fake_test.exs", line: 20, test_type: :test},
            time: 59_000_000
          }
        }
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      # Non-assertion errors get full formatted output too
      assert xml =~ ~r/<failure><!\[CDATA\[.*exit.*time out/s
    end)
  end

  test "no test_failure issue for failed test renders bare failure element" do
    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: [])
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      assert xml =~ ~r/<failure\/>/
    end)
  end

  test "failed test with infra issues includes note in failure and separate error" do
    issues = [
      %{
        type: :test_failure,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{
          test: %ExUnit.Test{
            name: :"test fails",
            module: FakeModule,
            state:
              {:failed, [{:error, %ExUnit.AssertionError{message: "Expected 1, got 2"}, []}]},
            tags: %{file: "test/fake_test.exs", line: 20, test_type: :test},
            time: 59_000_000
          }
        }
      },
      %{
        type: :crash,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "segfault"}
      },
      %{
        type: :sanitizer_report,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{server: "srv-2", report: "TSAN warning", kind: "data race"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")

      testcase_match =
        Regex.run(
          ~r/<testcase[^>]*name="test fails"[^>]*>(.*?)<\/testcase>/s,
          xml,
          capture: :all_but_first
        )

      assert [inner] = testcase_match

      # Failure element has the assertion message and a note about infra issues
      assert inner =~ ~r/<failure message="Expected 1, got 2">/
      assert inner =~ "Additional infrastructure issues:"
      assert inner =~ "- crash (srv-1)"
      assert inner =~ "- sanitizer report: data race (srv-2)"

      # Error element has full details
      assert inner =~ ~r/<error message="crash, sanitizer report: data race">/
      assert inner =~ "segfault"
      assert inner =~ "TSAN warning"
    end)
  end

  test "no system-err or infrastructure suite when result has no non-failure issues" do
    issues = [
      %{
        type: :test_failure,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{test: %{name: :"test fails", module: FakeModule}}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      refute xml =~ ~r/<system-err>/
      refute xml =~ ~r/<testsuite[^>]*name="_infrastructure_"/
    end)
  end

  test "multiple modules are sorted by name in output" do
    modules = %{
      ZModule => %{
        started_at: mod_started_at(),
        finished_at: mod_finished_at(),
        setup_finished_at: nil,
        teardown_started_at: nil,
        tests: [
          %{
            name: :"test z",
            outcome: :passed,
            duration_us: 1000,
            started_at: test1_started_at(),
            finished_at: test1_finished_at(),
            tags: %{}
          }
        ]
      },
      AModule => %{
        started_at: mod_started_at(),
        finished_at: mod_finished_at(),
        setup_finished_at: nil,
        teardown_started_at: nil,
        tests: [
          %{
            name: :"test a",
            outcome: :passed,
            duration_us: 1000,
            started_at: test1_started_at(),
            finished_at: test1_finished_at(),
            tags: %{}
          }
        ]
      }
    }

    with_tmp_dir(fn dir ->
      test_data = build_test_data(%{modules: modules})
      result = SuiteResult.build(test_data, [])
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      a_pos = :binary.match(xml, "AModule") |> elem(0)
      z_pos = :binary.match(xml, "ZModule") |> elem(0)
      assert a_pos < z_pos
    end)
  end

  test "module-scoped issues update testsuite test and error counts" do
    issues = [
      %{
        type: :crash,
        scope: {:module, FakeModule},
        confidence: :low,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "crash"}
      },
      %{
        type: :sanitizer_report,
        scope: {:module, FakeModule},
        confidence: nil,
        detail: %{server: "srv-2", report: "leak"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      # 2 real tests + 2 synthetic = 4
      assert xml =~ ~r/<testsuite[^>]*name="Elixir.FakeModule"[^>]*tests="4"/
      # 2 synthetic errors
      assert xml =~ ~r/<testsuite[^>]*name="Elixir.FakeModule"[^>]*errors="2"/
    end)
  end

  test "suite-scoped issues update testsuites test and error counts" do
    issues = [
      %{
        type: :crash,
        scope: :suite,
        confidence: :medium,
        detail: %{server: "srv-1", coredumps: [], crash_lines: "boom"}
      }
    ]

    with_tmp_dir(fn dir ->
      result = build_suite_result(issues: issues)
      SuiteResult.write_junit_xml(result, dir)

      xml = read_xml!(dir, "smoke.xml")
      # 2 real tests + 1 synthetic = 3
      assert xml =~ ~r/<testsuites[^>]*tests="3"/
      # 1 synthetic error
      assert xml =~ ~r/<testsuites[^>]*errors="1"/
    end)
  end
end
