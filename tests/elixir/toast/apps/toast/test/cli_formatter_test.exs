defmodule Toast.CLIFormatterTest do
  use ExUnit.Case, async: false

  import ExUnit.CaptureIO

  alias Toast.CLIFormatter

  defp make_test(overrides \\ %{}) do
    defaults = %{
      name: :"test something",
      module: FakeTest,
      state: nil,
      time: 25_000,
      tags: %{file: "test/fake_test.exs", line: 5, test_type: :test}
    }

    fields = Map.merge(defaults, overrides)

    %ExUnit.Test{
      name: fields.name,
      module: fields.module,
      state: fields.state,
      time: fields.time,
      tags: fields.tags
    }
  end

  defp make_module(name \\ FakeTest, state \\ nil) do
    %ExUnit.TestModule{name: name, state: state}
  end

  defp init_state(opts \\ []) do
    {:ok, state} = CLIFormatter.init(Keyword.merge([colors_enabled: false], opts))
    state
  end

  describe "init/1" do
    test "returns initial state with zeroed counters" do
      {:ok, state} = CLIFormatter.init(colors_enabled: false)

      assert state.counters == %{
               passed: 0,
               failed: 0,
               skipped: 0,
               excluded: 0,
               invalid: 0,
               total: 0
             }

      assert state.failures == []
      assert state.colors_enabled == false
    end

    test "detects colors from config" do
      {:ok, state} = CLIFormatter.init(colors_enabled: true)
      assert state.colors_enabled == true
    end
  end

  describe "suite_started" do
    test "records start time" do
      state = init_state()
      before = System.monotonic_time(:millisecond)
      {:noreply, new_state} = CLIFormatter.handle_cast({:suite_started, []}, state)
      assert new_state.suite_start_time >= before
    end
  end

  describe "module_started" do
    test "prints file header with module name" do
      state = init_state()

      output =
        capture_io(fn ->
          {:noreply, _state} =
            CLIFormatter.handle_cast({:module_started, make_module()}, state)
        end)

      assert output =~ "Running"
      assert output =~ "─"
    end

    test "resets module test count" do
      state = %{init_state() | module_test_count: 5}

      capture_io(fn ->
        {:noreply, new_state} =
          CLIFormatter.handle_cast({:module_started, make_module()}, state)

        send(self(), {:state, new_state})
      end)

      assert_received {:state, new_state}
      assert new_state.module_test_count == 0
    end
  end

  describe "test_started" do
    test "prints RUN line with test name" do
      state = init_state()
      test = make_test()

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_started, test}, state)
        end)

      assert output =~ "[ RUN        ]"
      assert output =~ "something"
    end

    test "strips 'test ' prefix from name" do
      state = init_state()
      test = make_test(%{name: :"test server version"})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_started, test}, state)
        end)

      assert output =~ "server version"
      refute output =~ "test server version"
    end
  end

  describe "test_finished — passed" do
    test "prints PASSED line with duration" do
      state = init_state()
      test = make_test(%{time: 50_000})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output =~ "[     PASSED ]"
      assert output =~ "something"
      assert output =~ "(50ms)"
    end

    test "increments passed counter" do
      state = init_state()
      test = make_test()

      capture_io(fn ->
        {:noreply, new_state} = CLIFormatter.handle_cast({:test_finished, test}, state)
        send(self(), {:state, new_state})
      end)

      assert_received {:state, new_state}
      assert new_state.counters.passed == 1
      assert new_state.counters.total == 1
    end
  end

  describe "test_finished — failed" do
    test "prints FAILED line" do
      state = init_state()

      error = %ExUnit.AssertionError{message: "Expected true, got false"}
      stacktrace = [{FakeTest, :"test fails", 1, [file: ~c"test/fake_test.exs", line: 10]}]

      test =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, error, stacktrace}]},
          time: 100_000
        })

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output =~ "[     FAILED ]"
      assert output =~ "fails"
      assert output =~ "(100ms)"
    end

    test "prints failure details" do
      state = init_state()

      error = %ExUnit.AssertionError{message: "Expected true, got false"}
      stacktrace = [{FakeTest, :"test fails", 1, [file: ~c"test/fake_test.exs", line: 10]}]

      test =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, error, stacktrace}]},
          time: 100_000
        })

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output =~ "Expected true, got false"
    end

    test "records failure for summary" do
      state = init_state()

      error = %ExUnit.AssertionError{message: "Expected true, got false"}
      stacktrace = [{FakeTest, :"test fails", 1, [file: ~c"test/fake_test.exs", line: 10]}]

      test =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, error, stacktrace}]},
          time: 100_000
        })

      capture_io(fn ->
        {:noreply, new_state} = CLIFormatter.handle_cast({:test_finished, test}, state)
        send(self(), {:state, new_state})
      end)

      assert_received {:state, new_state}
      assert new_state.counters.failed == 1
      assert length(new_state.failures) == 1
      assert new_state.failure_counter == 1
    end
  end

  describe "test_finished — skipped" do
    test "prints SKIPPED line" do
      state = init_state()
      test = make_test(%{state: {:skipped, "not implemented"}})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output =~ "[    SKIPPED ]"
      assert output =~ "something"
    end

    test "increments skipped counter" do
      state = init_state()
      test = make_test(%{state: {:skipped, "not implemented"}})

      capture_io(fn ->
        {:noreply, new_state} = CLIFormatter.handle_cast({:test_finished, test}, state)
        send(self(), {:state, new_state})
      end)

      assert_received {:state, new_state}
      assert new_state.counters.skipped == 1
    end
  end

  describe "test_finished — excluded" do
    test "prints EXCLUDED line" do
      state = init_state()
      test = make_test(%{state: {:excluded, "requires cluster"}})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output =~ "[   EXCLUDED ]"
    end

    test "increments excluded counter" do
      state = init_state()
      test = make_test(%{state: {:excluded, "requires cluster"}})

      capture_io(fn ->
        {:noreply, new_state} = CLIFormatter.handle_cast({:test_finished, test}, state)
        send(self(), {:state, new_state})
      end)

      assert_received {:state, new_state}
      assert new_state.counters.excluded == 1
    end
  end

  describe "test_finished — invalid" do
    test "prints INVALID line" do
      state = init_state()
      test = make_test(%{state: {:invalid, %ExUnit.TestModule{name: FakeTest, state: nil}}})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output =~ "[    INVALID ]"
    end
  end

  describe "module_finished" do
    test "prints module summary with test count and duration" do
      state = %{init_state() | module_test_count: 3, module_start_time: System.monotonic_time(:millisecond) - 200}

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:module_finished, make_module()}, state)
        end)

      assert output =~ "[------------]"
      assert output =~ "3 tests from"
      assert output =~ "FakeTest"
      assert output =~ "ms total)"
    end

    test "uses singular 'test' for count of 1" do
      state = %{init_state() | module_test_count: 1, module_start_time: System.monotonic_time(:millisecond)}

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:module_finished, make_module()}, state)
        end)

      assert output =~ "1 test from"
    end
  end

  describe "suite_finished" do
    test "prints session summary for all-passing suite" do
      state = %{
        init_state()
        | counters: %{passed: 3, failed: 0, skipped: 0, excluded: 0, invalid: 0, total: 3},
          suite_start_time: System.monotonic_time(:millisecond) - 1000
      }

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:suite_finished, %{async: 0, sync: 3_000_000}}, state)
        end)

      assert output =~ "PASSED"
      assert output =~ "3 tests"
      assert output =~ "[============]"
      assert output =~ "3 passed"
      assert output =~ "0 failed"
    end

    test "prints session summary with failures" do
      state = %{
        init_state()
        | counters: %{passed: 2, failed: 1, skipped: 0, excluded: 0, invalid: 0, total: 3},
          suite_start_time: System.monotonic_time(:millisecond) - 1000,
          failures: [make_test(%{name: :"test fails", state: {:failed, []}, time: 100_000})]
      }

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:suite_finished, %{async: 0, sync: 3_000_000}}, state)
        end)

      assert output =~ "FAILED"
      assert output =~ "1 failed"
      assert output =~ "Failed tests:"
    end

    test "includes skipped count when present" do
      state = %{
        init_state()
        | counters: %{passed: 2, failed: 0, skipped: 1, excluded: 0, invalid: 0, total: 3},
          suite_start_time: System.monotonic_time(:millisecond) - 1000
      }

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:suite_finished, %{async: 0, sync: 3_000_000}}, state)
        end)

      assert output =~ "1 skipped"
    end
  end

  describe "timestamp format" do
    test "output contains ISO 8601 timestamp with milliseconds" do
      state = init_state()
      test = make_test()

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_started, test}, state)
        end)

      # Should match pattern like 2026-02-15T14:30:45.123Z
      assert output =~ ~r/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z/
    end
  end

  describe "colors" do
    test "applies ANSI codes when colors enabled" do
      {:ok, state} = CLIFormatter.init(colors_enabled: true)
      test = make_test()

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      # Green ANSI code for PASSED
      assert output =~ "\e[32m"
    end

    test "no ANSI codes when colors disabled" do
      state = init_state()
      test = make_test()

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      refute output =~ "\e["
    end
  end

  describe "sigquit" do
    test "prints running tests" do
      state = init_state()
      running = [make_test(%{name: :"test slow thing"})]

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:sigquit, running}, state)
        end)

      assert output =~ "running tests"
      assert output =~ "FakeTest"
      assert output =~ "slow thing"
    end
  end

  describe "max_failures_reached" do
    test "prints abort message" do
      state = init_state()

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:max_failures_reached, 5}, state)
        end)

      assert output =~ "max-failures reached"
    end
  end

  describe "unknown messages" do
    test "ignored gracefully" do
      state = init_state()
      {:noreply, new_state} = CLIFormatter.handle_cast({:unknown_event, %{}}, state)
      assert new_state == state
    end
  end
end
