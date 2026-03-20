defmodule ToastTest.SuiteResultTest do
  use ExUnit.Case, async: true

  alias ToastTest.SuiteResult

  @suite_started_at ~U[2026-03-09 10:00:00Z]
  @suite_finished_at ~U[2026-03-09 10:05:00Z]

  @mod_started_at ~U[2026-03-09 10:00:01Z]
  @mod_finished_at ~U[2026-03-09 10:04:59Z]
  @setup_finished_at ~U[2026-03-09 10:00:02Z]
  @teardown_started_at ~U[2026-03-09 10:04:58Z]

  @test1_started_at ~U[2026-03-09 10:00:02Z]
  @test1_finished_at ~U[2026-03-09 10:01:00Z]
  @test2_started_at ~U[2026-03-09 10:01:01Z]
  @test2_finished_at ~U[2026-03-09 10:02:00Z]

  # --- Fixture builders ---

  defp build_test_data(overrides \\ %{}) do
    defaults = %{
      suite: "smoke",
      started_at: @suite_started_at,
      finished_at: @suite_finished_at,
      times_us: %{async: 0, load: 5000, run: 300_000_000},
      modules: %{
        FakeModule => %{
          started_at: @mod_started_at,
          finished_at: @mod_finished_at,
          setup_finished_at: @setup_finished_at,
          teardown_started_at: @teardown_started_at,
          tests: [
            %{
              name: :"test passes",
              outcome: :passed,
              duration_us: 58_000_000,
              started_at: @test1_started_at,
              finished_at: @test1_finished_at,
              tags: %{file: "test/fake_test.exs", line: 10}
            },
            %{
              name: :"test fails",
              outcome: :failed,
              duration_us: 59_000_000,
              started_at: @test2_started_at,
              finished_at: @test2_finished_at,
              tags: %{file: "test/fake_test.exs", line: 20}
            }
          ]
        }
      }
    }

    Map.merge(defaults, overrides)
  end

  defp build_issues do
    [
      %{
        type: :test_failure,
        scope: {:test, FakeModule, :"test fails"},
        confidence: :high,
        detail: %{test: %{name: :"test fails", module: FakeModule}}
      },
      %{
        type: :crash,
        scope: {:module, FakeModule},
        confidence: :low,
        detail: %{
          server: "srv-1",
          coredumps: [%{path: "/tmp/core.1234", signal: "SIGABRT", threads: []}],
          logs: "some log output"
        }
      }
    ]
  end

  defp build_sanitizer_issue do
    %{
      type: :sanitizer_report,
      scope: :suite,
      confidence: nil,
      detail: %{server: "srv-1", report: "ASAN detected leak"}
    }
  end

  defp build_suite_result(opts \\ []) do
    test_data = Keyword.get(opts, :test_data, build_test_data())
    issues = Keyword.get(opts, :issues, build_issues())
    events = Keyword.get(opts, :events, [])
    deployments = Keyword.get(opts, :deployments, %{})
    SuiteResult.build(test_data, issues, events: events, deployments: deployments)
  end

  defp with_tmp_dir(fun) do
    dir = Path.join(System.tmp_dir!(), "suite_result_test_#{:erlang.unique_integer([:positive])}")
    File.mkdir_p!(dir)

    try do
      fun.(dir)
    after
      File.rm_rf!(dir)
    end
  end

  # --- build/3 ---

  describe "build/3" do
    test "produces a SuiteResult struct with version 1" do
      result = build_suite_result()

      assert %SuiteResult{} = result
      assert result.version == 1
    end

    test "populates suite name from test_data" do
      result = build_suite_result()

      assert result.suite == "smoke"
    end

    test "populates timestamps from test_data" do
      result = build_suite_result()

      assert result.started_at == @suite_started_at
      assert result.finished_at == @suite_finished_at
    end

    test "populates times_us from test_data" do
      result = build_suite_result()

      assert result.times_us == %{async: 0, load: 5000, run: 300_000_000}
    end

    test "populates modules from test_data" do
      result = build_suite_result()

      assert map_size(result.modules) == 1
      assert %{tests: [_, _]} = result.modules[FakeModule]
    end

    test "populates issues" do
      result = build_suite_result()

      assert length(result.issues) == 2
      assert Enum.any?(result.issues, &(&1.type == :test_failure))
      assert Enum.any?(result.issues, &(&1.type == :crash))
    end

    test "defaults events to empty list when not provided" do
      test_data = build_test_data()
      result = SuiteResult.build(test_data, [])

      assert result.events == []
    end

    test "populates events when provided" do
      events = [%{event: :server_started, server_id: "s1", timestamp: @test1_started_at}]
      result = build_suite_result(events: events)

      assert result.events == events
    end

    test "defaults warnings to empty list when not provided" do
      test_data = build_test_data()
      result = SuiteResult.build(test_data, [])

      assert result.warnings == []
    end

    test "populates warnings when provided" do
      test_data = build_test_data()
      warnings = ["Coredump discovery may not work"]
      result = SuiteResult.build(test_data, [], warnings: warnings)

      assert result.warnings == warnings
    end

    test "handles empty modules" do
      test_data = build_test_data(%{modules: %{}})
      result = SuiteResult.build(test_data, [])

      assert result.modules == %{}
    end

    test "handles empty issues" do
      result = build_suite_result(issues: [])

      assert result.issues == []
    end

    test "preserves multiple modules" do
      modules =
        build_test_data().modules
        |> Map.put(OtherModule, %{
          started_at: @mod_started_at,
          finished_at: @mod_finished_at,
          setup_finished_at: nil,
          teardown_started_at: nil,
          tests: [
            %{
              name: :"test other",
              outcome: :skipped,
              duration_us: 0,
              started_at: @test1_started_at,
              finished_at: @test1_finished_at,
              tags: %{}
            }
          ]
        })

      test_data = build_test_data(%{modules: modules})
      result = SuiteResult.build(test_data, [])

      assert map_size(result.modules) == 2
      assert Map.has_key?(result.modules, FakeModule)
      assert Map.has_key?(result.modules, OtherModule)
    end
  end

  # --- write_outcomes_json/2 ---

  describe "write_outcomes_json/2" do
    test "creates a JSON file at the expected path" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_outcomes_json(result, dir)

        assert File.exists?(Path.join(dir, "outcomes.json"))
      end)
    end

    test "produces valid JSON" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_outcomes_json(result, dir)

        content = File.read!(Path.join(dir, "outcomes.json"))
        assert {:ok, _} = json_decode(content)
      end)
    end

    test "includes suite name" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_outcomes_json(result, dir)

        decoded = read_json!(dir, "outcomes.json")
        assert decoded["suite"] == "smoke"
      end)
    end

    test "includes ISO 8601 timestamps" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_outcomes_json(result, dir)

        decoded = read_json!(dir, "outcomes.json")
        assert decoded["started_at"] == "2026-03-09T10:00:00Z"
        assert decoded["finished_at"] == "2026-03-09T10:05:00Z"
      end)
    end

    test "includes duration_us from run time" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_outcomes_json(result, dir)

        decoded = read_json!(dir, "outcomes.json")
        assert decoded["duration_us"] == 300_000_000
      end)
    end

    test "includes correct summary counts" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_outcomes_json(result, dir)

        decoded = read_json!(dir, "outcomes.json")
        summary = decoded["summary"]
        assert summary["passed"] == 1
        assert summary["failed"] == 1
        assert summary["skipped"] == 0
        assert summary["excluded"] == 0
        assert summary["invalid"] == 0
      end)
    end

    test "includes flat test list with correct fields" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_outcomes_json(result, dir)

        decoded = read_json!(dir, "outcomes.json")
        tests = decoded["tests"]
        assert length(tests) == 2

        passed = Enum.find(tests, &(&1["outcome"] == "passed"))
        assert passed["module"] == "Elixir.FakeModule"
        assert passed["name"] == "test passes"
        assert passed["duration_us"] == 58_000_000
      end)
    end

    test "handles all outcome types in summary" do
      modules = %{
        AllOutcomes => %{
          started_at: @mod_started_at,
          finished_at: @mod_finished_at,
          setup_finished_at: nil,
          teardown_started_at: nil,
          tests:
            for {outcome, i} <- Enum.with_index([:passed, :failed, :skipped, :excluded, :invalid]) do
              %{
                name: :"test #{outcome}",
                outcome: outcome,
                duration_us: 1000 * (i + 1),
                started_at: @test1_started_at,
                finished_at: @test1_finished_at,
                tags: %{}
              }
            end
        }
      }

      with_tmp_dir(fn dir ->
        test_data = build_test_data(%{modules: modules})
        result = SuiteResult.build(test_data, [])
        SuiteResult.write_outcomes_json(result, dir)

        decoded = read_json!(dir, "outcomes.json")
        summary = decoded["summary"]
        assert summary["passed"] == 1
        assert summary["failed"] == 1
        assert summary["skipped"] == 1
        assert summary["excluded"] == 1
        assert summary["invalid"] == 1
      end)
    end

    test "handles empty modules" do
      with_tmp_dir(fn dir ->
        test_data = build_test_data(%{modules: %{}})
        result = SuiteResult.build(test_data, [])
        SuiteResult.write_outcomes_json(result, dir)

        decoded = read_json!(dir, "outcomes.json")
        assert decoded["tests"] == []
        assert decoded["summary"]["passed"] == 0
      end)
    end
  end

  # --- write_diagnostics_etf/2 ---

  describe "write_diagnostics_etf/2" do
    test "creates an ETF file at the expected path" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_diagnostics_etf(result, dir)

        assert File.exists?(Path.join(dir, "smoke.diagnostics.etf"))
      end)
    end

    test "roundtrips through binary_to_term" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_diagnostics_etf(result, dir)

        binary = File.read!(Path.join(dir, "smoke.diagnostics.etf"))
        restored = :erlang.binary_to_term(binary)

        assert %SuiteResult{} = restored
        assert restored.version == result.version
        assert restored.suite == result.suite
        assert restored.started_at == result.started_at
        assert restored.finished_at == result.finished_at
        assert restored.times_us == result.times_us
        assert restored.modules == result.modules
        assert restored.issues == result.issues
        assert restored.events == result.events
      end)
    end

    test "output is compressed (smaller than uncompressed)" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_diagnostics_etf(result, dir)

        compressed = File.read!(Path.join(dir, "smoke.diagnostics.etf"))
        uncompressed = :erlang.term_to_binary(result)

        assert byte_size(compressed) <= byte_size(uncompressed)
      end)
    end

    test "preserves all issue types through roundtrip" do
      issues = build_issues() ++ [build_sanitizer_issue()]

      with_tmp_dir(fn dir ->
        result = build_suite_result(issues: issues)
        SuiteResult.write_diagnostics_etf(result, dir)

        binary = File.read!(Path.join(dir, "smoke.diagnostics.etf"))
        restored = :erlang.binary_to_term(binary)

        assert length(restored.issues) == 3
        types = Enum.map(restored.issues, & &1.type) |> Enum.sort()
        assert types == [:crash, :sanitizer_report, :test_failure]
      end)
    end

    test "preserves events through roundtrip" do
      events = [
        %{event: :server_started, server_id: "s1", pid: 1001, timestamp: @test1_started_at},
        %{event: :server_stopped, server_id: "s1", pid: 1001, timestamp: @test1_finished_at}
      ]

      with_tmp_dir(fn dir ->
        result = build_suite_result(events: events)
        SuiteResult.write_diagnostics_etf(result, dir)

        binary = File.read!(Path.join(dir, "smoke.diagnostics.etf"))
        restored = :erlang.binary_to_term(binary)

        assert restored.events == events
      end)
    end
  end

  # --- write_all/2 ---

  describe "write_all/2" do
    test "creates all three output files" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_all(result, dir)

        assert File.exists?(Path.join(dir, "outcomes.json"))
        assert File.exists?(Path.join(dir, "smoke.diagnostics.etf"))
        assert File.exists?(Path.join(dir, "smoke.xml"))
      end)
    end

    test "creates the result directory if it does not exist" do
      with_tmp_dir(fn dir ->
        nested = Path.join(dir, "nested/results")
        result = build_suite_result()
        SuiteResult.write_all(result, nested)

        assert File.exists?(Path.join(nested, "outcomes.json"))
        assert File.exists?(Path.join(nested, "smoke.diagnostics.etf"))
        assert File.exists?(Path.join(nested, "smoke.xml"))
      end)
    end

    test "outcomes.json is valid JSON" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_all(result, dir)

        content = File.read!(Path.join(dir, "outcomes.json"))
        assert {:ok, _} = json_decode(content)
      end)
    end

    test "diagnostics ETF roundtrips correctly" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_all(result, dir)

        binary = File.read!(Path.join(dir, "smoke.diagnostics.etf"))
        restored = :erlang.binary_to_term(binary)
        assert restored.suite == "smoke"
      end)
    end

    test "JUnit XML is well-formed" do
      with_tmp_dir(fn dir ->
        result = build_suite_result()
        SuiteResult.write_all(result, dir)

        xml = File.read!(Path.join(dir, "smoke.xml"))
        assert String.starts_with?(xml, ~s(<?xml version="1.0"))
        assert xml =~ ~r/<\/testsuites>/
      end)
    end
  end

  # --- JSON helpers ---

  defp json_decode(content) do
    {:ok, :json.decode(content)}
  rescue
    e -> {:error, e}
  end

  defp read_json!(dir, filename) do
    dir
    |> Path.join(filename)
    |> File.read!()
    |> :json.decode()
  end
end
