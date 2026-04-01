defmodule ToastTest.CLIFormatterTest do
  use ExUnit.Case, async: true

  import ExUnit.CaptureIO

  alias ToastTest.CLIFormatter

  import Toast.FormatterTestHelpers, only: [make_test: 0, make_test: 1]

  defp make_module(name \\ FakeTest, state \\ nil) do
    %ExUnit.TestModule{name: name, state: state}
  end

  defp init_state(opts \\ []) do
    {:ok, state} = CLIFormatter.init(Keyword.merge([colors_enabled: false], opts))
    state
  end

  # Simulate module_started to set pending_module (header is now deferred)
  defp with_module_started(state, mod \\ make_module()) do
    {:noreply, state} = CLIFormatter.handle_cast({:module_started, mod}, state)
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
    test "defers header printing (does not print immediately)" do
      state = init_state()

      output =
        capture_io(fn ->
          {:noreply, _state} =
            CLIFormatter.handle_cast({:module_started, make_module()}, state)
        end)

      assert output == ""
    end

    test "sets pending module and resets counters" do
      state = %{init_state() | module_test_count: 5}
      mod = make_module()

      {:noreply, new_state} = CLIFormatter.handle_cast({:module_started, mod}, state)

      assert new_state.module_test_count == 0
      assert new_state.module_skipped_count == 0
      assert new_state.pending_module == mod
      assert new_state.module_header_printed == false
    end
  end

  describe "test_started" do
    test "prints module header and RUN line on first test" do
      state = init_state() |> with_module_started()
      test = make_test()

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_started, test}, state)
        end)

      assert output =~ "Running"
      assert output =~ "─"
      assert output =~ "[ RUN        ]"
      assert output =~ "something"
    end

    test "does not reprint header on subsequent tests" do
      state = %{(init_state() |> with_module_started()) | module_header_printed: true}
      test = make_test()

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_started, test}, state)
        end)

      refute output =~ "Running"
      assert output =~ "[ RUN        ]"
    end

    test "strips 'test ' prefix from name" do
      state = init_state() |> with_module_started()
      test = make_test(%{name: :"test server version"})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_started, test}, state)
        end)

      assert output =~ "server version"
      refute output =~ "test server version"
    end

    test "suppresses RUN for excluded tests" do
      state = init_state() |> with_module_started()
      test = make_test(%{state: {:excluded, "requires cluster"}})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_started, test}, state)
        end)

      assert output == ""
    end

    test "suppresses RUN for abort-skipped tests" do
      state = init_state() |> with_module_started()
      test = make_test(%{state: {:skipped, "Suite aborted: Server crashed: srv-1"}})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_started, test}, state)
        end)

      assert output == ""
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
      assert new_state.failure_counter == 1
    end
  end

  describe "test_finished — skipped" do
    test "prints SKIPPED line for @tag :skip" do
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

    test "suppresses abort-skipped tests when module header not printed" do
      state = init_state() |> with_module_started()
      test = make_test(%{state: {:skipped, "Suite aborted: Server crashed: srv-1"}})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output == ""
    end

    test "shows abort-skipped tests when module header was printed" do
      state = %{(init_state() |> with_module_started()) | module_header_printed: true}
      test = make_test(%{state: {:skipped, "Suite aborted: Server crashed: srv-1"}})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output =~ "[    SKIPPED ]"
      assert output =~ "something"
    end

    test "tracks abort-skipped count" do
      state = init_state() |> with_module_started()
      test = make_test(%{state: {:skipped, "Suite aborted: Server crashed: srv-1"}})

      capture_io(fn ->
        {:noreply, new_state} = CLIFormatter.handle_cast({:test_finished, test}, state)
        send(self(), {:state, new_state})
      end)

      assert_received {:state, new_state}
      assert new_state.module_skipped_count == 1
      assert new_state.counters.skipped == 1
    end
  end

  describe "test_finished — excluded" do
    test "silently counts filter-excluded tests" do
      state = init_state()
      test = make_test(%{state: {:excluded, "requires cluster"}})

      output =
        capture_io(fn ->
          {:noreply, _} = CLIFormatter.handle_cast({:test_finished, test}, state)
        end)

      assert output == ""
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
    test "prints module summary when header was printed" do
      state = %{
        init_state()
        | pending_module: make_module(),
          module_header_printed: true,
          module_test_count: 3,
          module_start_time: System.monotonic_time(:millisecond) - 200
      }

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

    test "includes skipped count in summary for partially-run modules" do
      state = %{
        init_state()
        | pending_module: make_module(),
          module_header_printed: true,
          module_test_count: 5,
          module_skipped_count: 3,
          module_start_time: System.monotonic_time(:millisecond) - 200
      }

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:module_finished, make_module()}, state)
        end)

      assert output =~ "[------------]"
      assert output =~ "5 tests from"
      assert output =~ "3 skipped"
    end

    test "uses singular 'test' for count of 1" do
      state = %{
        init_state()
        | pending_module: make_module(),
          module_header_printed: true,
          module_test_count: 1,
          module_start_time: System.monotonic_time(:millisecond)
      }

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:module_finished, make_module()}, state)
        end)

      assert output =~ "1 test from"
    end

    test "prints single SKIPPED line for fully-skipped modules" do
      state = %{
        init_state()
        | pending_module: make_module(),
          module_header_printed: false,
          module_test_count: 5,
          module_skipped_count: 5,
          module_start_time: System.monotonic_time(:millisecond)
      }

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:module_finished, make_module()}, state)
        end)

      assert output =~ "[    SKIPPED ]"
      assert output =~ "5 tests from"
      assert output =~ "FakeTest"
      refute output =~ "[------------]"
      refute output =~ "Running"
    end

    test "produces no output for purely filter-excluded modules" do
      state = %{
        init_state()
        | pending_module: make_module(),
          module_header_printed: false,
          module_test_count: 3,
          module_skipped_count: 0,
          module_start_time: System.monotonic_time(:millisecond)
      }

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:module_finished, make_module()}, state)
        end)

      assert output == ""
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
          suite_start_time: System.monotonic_time(:millisecond) - 1000
      }

      output =
        capture_io(fn ->
          {:noreply, _} =
            CLIFormatter.handle_cast({:suite_finished, %{async: 0, sync: 3_000_000}}, state)
        end)

      assert output =~ "FAILED"
      assert output =~ "1 failed"
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

  describe "print_failure_summary/1" do
    test "prints nothing for empty list" do
      output = capture_io(fn -> CLIFormatter.print_failure_summary([]) end)
      assert output == ""
    end

    test "prints failure banner and details" do
      failed_test =
        make_test(%{
          name: :"test something fails",
          state:
            {:failed,
             [
               {:error,
                %ExUnit.AssertionError{
                  message: "Expected true, got false",
                  expr: {:assert, [], [false]},
                  left: false,
                  right: :ex_unit_no_meaningful_value
                }, []}
             ]},
          time: 100_000
        })

      output = capture_io(fn -> CLIFormatter.print_failure_summary([failed_test]) end)
      assert output =~ "TEST FAILURES"
      assert output =~ "something fails"
    end
  end

  describe "timestamp format" do
    test "output contains ISO 8601 timestamp with milliseconds" do
      state = init_state() |> with_module_started()
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
