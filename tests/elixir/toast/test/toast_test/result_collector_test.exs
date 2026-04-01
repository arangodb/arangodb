defmodule ToastTest.ResultCollectorTest do
  use ExUnit.Case, async: true

  alias ToastTest.ResultCollector

  import Toast.FormatterTestHelpers, only: [make_test: 0, make_test: 1]

  defp init(opts \\ []), do: ResultCollector.init(opts)

  defp cast(state, event), do: ResultCollector.handle_cast(event, state)

  defp call_get_data(state) do
    {:reply, data, _state} = ResultCollector.handle_call(:get_data, self(), state)
    data
  end

  defp module_event(type, module) do
    {type, %ExUnit.TestModule{name: module, state: nil}}
  end

  defp test_started_event(test) do
    {:test_started, test}
  end

  defp test_finished_event(test) do
    {:test_finished, test}
  end

  # --- init/1 ---

  describe "init/1" do
    test "initial state has empty modules, module_timestamps, and test_start_times" do
      {:ok, state} = init()

      assert state.modules == %{}
      assert state.module_timestamps == %{}
      assert state.test_start_times == %{}
      assert state.failures == []
      assert state.finished_at == nil
      assert state.times_us == nil
    end

    test "config is stored" do
      {:ok, state} = init(suite: "smoke", timeout: 5000)

      assert state.config == [suite: "smoke", timeout: 5000]
    end
  end

  # --- :suite_started ---

  describe ":suite_started event" do
    test "updates suite_started_at" do
      {:ok, state} = init()
      early = state.suite_started_at

      Process.sleep(10)
      {:noreply, new_state} = cast(state, {:suite_started, []})

      assert DateTime.compare(new_state.suite_started_at, early) == :gt
    end
  end

  # --- :module_started ---

  describe ":module_started event" do
    test "records module started_at in module_timestamps" do
      {:ok, state} = init()

      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))

      assert %{started_at: %DateTime{}} = state.module_timestamps[FakeTest]
    end

    test "does not overwrite existing timestamps for same module" do
      {:ok, state} = init()

      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))
      first_started = state.module_timestamps[FakeTest].started_at

      {:noreply, state} = cast(state, module_event(:module_started, OtherTest))

      assert state.module_timestamps[FakeTest].started_at == first_started
      assert %{started_at: %DateTime{}} = state.module_timestamps[OtherTest]
    end
  end

  # --- :module_finished ---

  describe ":module_finished event" do
    test "records module finished_at in module_timestamps" do
      {:ok, state} = init()

      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))
      {:noreply, state} = cast(state, module_event(:module_finished, FakeTest))

      ts = state.module_timestamps[FakeTest]
      assert %DateTime{} = ts.finished_at
      assert DateTime.compare(ts.finished_at, ts.started_at) in [:gt, :eq]
    end
  end

  # --- :test_started ---

  describe ":test_started event" do
    test "records test start time" do
      {:ok, state} = init()
      test = make_test()

      {:noreply, state} = cast(state, test_started_event(test))

      key = {test.module, test.name}
      assert %DateTime{} = state.test_start_times[key]
    end

    test "first test_started for a module sets setup_finished_at" do
      {:ok, state} = init()

      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))

      test1 = make_test(%{name: :"test first"})
      {:noreply, state} = cast(state, test_started_event(test1))

      assert %DateTime{} = state.module_timestamps[FakeTest].setup_finished_at

      # Second test does not change setup_finished_at
      setup_ts = state.module_timestamps[FakeTest].setup_finished_at
      Process.sleep(10)

      test2 = make_test(%{name: :"test second"})
      {:noreply, state} = cast(state, test_started_event(test2))

      assert state.module_timestamps[FakeTest].setup_finished_at == setup_ts
    end
  end

  # --- :test_finished — outcome types ---

  describe ":test_finished — passed test" do
    test "produces a plain map with name as atom, outcome :passed, duration, timestamps, tags" do
      {:ok, state} = init()
      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))

      test = make_test()
      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))

      assert [result] = state.modules[FakeTest]
      assert is_map(result)
      refute Map.has_key?(result, :__struct__)
      assert result.name == :"test something"
      assert result.outcome == :passed
      assert result.duration_us == 25_000
      assert %DateTime{} = result.started_at
      assert %DateTime{} = result.finished_at
      assert result.tags == %{file: "test/fake_test.exs", line: 5}
    end
  end

  describe ":test_finished — failed test" do
    test "produces a plain map with outcome :failed, no failure details in test result" do
      {:ok, state} = init()
      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))

      error = %ExUnit.AssertionError{message: "Expected true, got false"}
      stacktrace = [{FakeTest, :"test fails", 1, [file: ~c"test/fake_test.exs", line: 10]}]

      test =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, error, stacktrace}]},
          time: 100_000
        })

      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))

      assert [result] = state.modules[FakeTest]
      assert result.outcome == :failed
      refute Map.has_key?(result, :failure)
    end
  end

  describe ":test_finished — skipped test" do
    test "outcome is :skipped" do
      {:ok, state} = init()
      test = make_test(%{state: {:skipped, "not implemented yet"}})

      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))

      assert [result] = state.modules[FakeTest]
      assert result.outcome == :skipped
    end
  end

  describe ":test_finished — excluded test" do
    test "outcome is :excluded" do
      {:ok, state} = init()
      test = make_test(%{state: {:excluded, "requires cluster"}})

      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))

      assert [result] = state.modules[FakeTest]
      assert result.outcome == :excluded
    end
  end

  describe ":test_finished — invalid test" do
    test "outcome is :invalid" do
      {:ok, state} = init()
      test = make_test(%{state: {:invalid, %ExUnit.TestModule{name: FakeTest, state: nil}}})

      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))

      assert [result] = state.modules[FakeTest]
      assert result.outcome == :invalid
    end
  end

  describe ":test_finished — failed test added to failures list" do
    test "failed test is added to failures as raw ExUnit.Test struct" do
      {:ok, state} = init()

      test =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "nope"}, []}]},
          time: 100_000
        })

      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))

      assert [failure] = state.failures
      assert %ExUnit.Test{} = failure
      assert failure.name == :"test fails"
    end

    test "passed test is not added to failures" do
      {:ok, state} = init()
      test = make_test()

      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))

      assert state.failures == []
    end
  end

  # --- :test_finished — teardown_started_at tracking ---

  describe ":test_finished — teardown_started_at tracking" do
    test "last test_finished for a module updates teardown_started_at" do
      {:ok, state} = init()
      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))

      test1 = make_test(%{name: :"test first", time: 10_000})
      {:noreply, state} = cast(state, test_started_event(test1))
      {:noreply, state} = cast(state, test_finished_event(test1))

      first_teardown = state.module_timestamps[FakeTest].teardown_started_at
      assert %DateTime{} = first_teardown

      Process.sleep(10)
      test2 = make_test(%{name: :"test second", time: 20_000})
      {:noreply, state} = cast(state, test_started_event(test2))
      {:noreply, state} = cast(state, test_finished_event(test2))

      second_teardown = state.module_timestamps[FakeTest].teardown_started_at
      assert DateTime.compare(second_teardown, first_teardown) == :gt
    end
  end

  # --- :suite_finished + get_data/1 ---

  describe ":suite_finished event + get_data/1" do
    test "produces complete data map with all fields" do
      {:ok, state} = init(suite: "smoke")
      {:noreply, state} = cast(state, {:suite_started, []})
      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))

      test1 = make_test(%{name: :"test first", time: 10_000})
      test2 = make_test(%{name: :"test second", time: 20_000})

      {:noreply, state} = cast(state, test_started_event(test1))
      {:noreply, state} = cast(state, test_finished_event(test1))
      {:noreply, state} = cast(state, test_started_event(test2))
      {:noreply, state} = cast(state, test_finished_event(test2))

      {:noreply, state} = cast(state, module_event(:module_finished, FakeTest))

      times_us = %{async: 0, sync: 30_000}
      {:noreply, state} = cast(state, {:suite_finished, times_us})

      data = call_get_data(state)

      assert data.suite == "smoke"
      assert %DateTime{} = data.started_at
      assert %DateTime{} = data.finished_at
      assert data.times_us == times_us
      assert is_map(data.modules)
      assert Map.has_key?(data.modules, FakeTest)
      assert is_list(data.failures)
    end

    test "tests are in execution order (not reversed)" do
      {:ok, state} = init(suite: "order")
      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))

      test1 = make_test(%{name: :"test first", time: 10_000})
      test2 = make_test(%{name: :"test second", time: 20_000})
      test3 = make_test(%{name: :"test third", time: 30_000})

      {:noreply, state} = cast(state, test_started_event(test1))
      {:noreply, state} = cast(state, test_finished_event(test1))
      {:noreply, state} = cast(state, test_started_event(test2))
      {:noreply, state} = cast(state, test_finished_event(test2))
      {:noreply, state} = cast(state, test_started_event(test3))
      {:noreply, state} = cast(state, test_finished_event(test3))

      {:noreply, state} = cast(state, module_event(:module_finished, FakeTest))
      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 60_000}})

      data = call_get_data(state)
      names = Enum.map(data.modules[FakeTest].tests, & &1.name)
      assert names == [:"test first", :"test second", :"test third"]
    end

    test "modules have correct timestamps from module and test events" do
      {:ok, state} = init(suite: "timestamps")
      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))

      test = make_test(%{name: :"test only", time: 10_000})
      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))

      {:noreply, state} = cast(state, module_event(:module_finished, FakeTest))
      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 10_000}})

      data = call_get_data(state)
      mod = data.modules[FakeTest]

      # started_at and finished_at come from module events
      assert %DateTime{} = mod.started_at
      assert %DateTime{} = mod.finished_at
      assert DateTime.compare(mod.finished_at, mod.started_at) in [:gt, :eq]

      # setup_finished_at comes from first test_started
      assert %DateTime{} = mod.setup_finished_at
      assert DateTime.compare(mod.setup_finished_at, mod.started_at) in [:gt, :eq]

      # teardown_started_at comes from last test_finished
      assert %DateTime{} = mod.teardown_started_at
      assert DateTime.compare(mod.teardown_started_at, mod.setup_finished_at) in [:gt, :eq]
      assert DateTime.compare(mod.finished_at, mod.teardown_started_at) in [:gt, :eq]
    end

    test "suite name comes from config :suite option" do
      {:ok, state} = init(suite: "replication2")
      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 0}})

      data = call_get_data(state)
      assert data.suite == "replication2"
    end

    test "suite name is nil when not configured" do
      {:ok, state} = init()
      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 0}})

      data = call_get_data(state)
      assert data.suite == nil
    end
  end

  # --- Multiple modules ---

  describe "multiple modules" do
    test "tests from different modules are correctly separated" do
      {:ok, state} = init(suite: "multi")

      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))
      test1 = make_test(%{name: :"test alpha", time: 10_000})
      {:noreply, state} = cast(state, test_started_event(test1))
      {:noreply, state} = cast(state, test_finished_event(test1))
      {:noreply, state} = cast(state, module_event(:module_finished, FakeTest))

      {:noreply, state} = cast(state, module_event(:module_started, OtherTest))
      test2 = make_test(%{name: :"test beta", module: OtherTest, time: 20_000})
      {:noreply, state} = cast(state, test_started_event(test2))
      {:noreply, state} = cast(state, test_finished_event(test2))
      {:noreply, state} = cast(state, module_event(:module_finished, OtherTest))

      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 30_000}})

      data = call_get_data(state)

      assert map_size(data.modules) == 2
      assert [%{name: :"test alpha"}] = data.modules[FakeTest].tests
      assert [%{name: :"test beta"}] = data.modules[OtherTest].tests
    end

    test "each module has its own timestamps" do
      {:ok, state} = init(suite: "multi-ts")

      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))
      test1 = make_test(%{name: :"test one", time: 10_000})
      {:noreply, state} = cast(state, test_started_event(test1))
      {:noreply, state} = cast(state, test_finished_event(test1))
      {:noreply, state} = cast(state, module_event(:module_finished, FakeTest))

      Process.sleep(10)

      {:noreply, state} = cast(state, module_event(:module_started, OtherTest))
      test2 = make_test(%{name: :"test two", module: OtherTest, time: 20_000})
      {:noreply, state} = cast(state, test_started_event(test2))
      {:noreply, state} = cast(state, test_finished_event(test2))
      {:noreply, state} = cast(state, module_event(:module_finished, OtherTest))

      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 30_000}})

      data = call_get_data(state)

      fake = data.modules[FakeTest]
      other = data.modules[OtherTest]

      # Each module has independent timestamps
      assert DateTime.compare(other.started_at, fake.started_at) == :gt
      assert %DateTime{} = fake.setup_finished_at
      assert %DateTime{} = other.setup_finished_at
      assert %DateTime{} = fake.teardown_started_at
      assert %DateTime{} = other.teardown_started_at
    end
  end

  # --- Module timestamps edge cases ---

  describe "module timestamps edge cases" do
    test "module with no tests: setup_finished_at and teardown_started_at are nil" do
      {:ok, state} = init(suite: "empty-mod")

      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))
      {:noreply, state} = cast(state, module_event(:module_finished, FakeTest))

      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 0}})

      data = call_get_data(state)
      mod = data.modules[FakeTest]

      assert %DateTime{} = mod.started_at
      assert %DateTime{} = mod.finished_at
      assert mod.setup_finished_at == nil
      assert mod.teardown_started_at == nil
      assert mod.tests == []
    end

    test "module with one test: setup_finished_at and teardown_started_at are both set" do
      {:ok, state} = init(suite: "single-test")

      {:noreply, state} = cast(state, module_event(:module_started, FakeTest))
      test = make_test(%{name: :"test only", time: 10_000})
      {:noreply, state} = cast(state, test_started_event(test))
      {:noreply, state} = cast(state, test_finished_event(test))
      {:noreply, state} = cast(state, module_event(:module_finished, FakeTest))

      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 10_000}})

      data = call_get_data(state)
      mod = data.modules[FakeTest]

      assert %DateTime{} = mod.setup_finished_at
      assert %DateTime{} = mod.teardown_started_at
      # Both derived from the same test's events
      assert DateTime.compare(mod.teardown_started_at, mod.setup_finished_at) in [:gt, :eq]
    end
  end

  # --- Failures tracking ---

  describe "failures tracking" do
    test "only failed tests appear in failures list" do
      {:ok, state} = init(suite: "failures")

      passed = make_test(%{name: :"test passes", time: 10_000})
      skipped = make_test(%{name: :"test skipped", state: {:skipped, "skip"}, time: 0})

      failed =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "nope"}, []}]},
          time: 50_000
        })

      excluded = make_test(%{name: :"test excluded", state: {:excluded, "no"}, time: 0})

      {:noreply, state} = cast(state, test_started_event(passed))
      {:noreply, state} = cast(state, test_finished_event(passed))
      {:noreply, state} = cast(state, test_started_event(skipped))
      {:noreply, state} = cast(state, test_finished_event(skipped))
      {:noreply, state} = cast(state, test_started_event(failed))
      {:noreply, state} = cast(state, test_finished_event(failed))
      {:noreply, state} = cast(state, test_started_event(excluded))
      {:noreply, state} = cast(state, test_finished_event(excluded))

      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 60_000}})

      data = call_get_data(state)
      assert length(data.failures) == 1
      assert hd(data.failures).name == :"test fails"
    end

    test "failures are in execution order" do
      {:ok, state} = init(suite: "fail-order")

      fail1 =
        make_test(%{
          name: :"test fail first",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "a"}, []}]},
          time: 10_000
        })

      fail2 =
        make_test(%{
          name: :"test fail second",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "b"}, []}]},
          time: 20_000
        })

      {:noreply, state} = cast(state, test_started_event(fail1))
      {:noreply, state} = cast(state, test_finished_event(fail1))
      {:noreply, state} = cast(state, test_started_event(fail2))
      {:noreply, state} = cast(state, test_finished_event(fail2))

      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 30_000}})

      data = call_get_data(state)
      failure_names = Enum.map(data.failures, & &1.name)
      assert failure_names == [:"test fail first", :"test fail second"]
    end

    test "failures contain raw ExUnit.Test structs" do
      {:ok, state} = init(suite: "raw-failures")

      failed =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "boom"}, []}]},
          time: 100_000
        })

      {:noreply, state} = cast(state, test_started_event(failed))
      {:noreply, state} = cast(state, test_finished_event(failed))

      {:noreply, state} = cast(state, {:suite_finished, %{async: 0, sync: 100_000}})

      data = call_get_data(state)
      assert [%ExUnit.Test{} = failure] = data.failures
      assert failure.name == :"test fails"
      assert {:failed, _} = failure.state
    end
  end
end
