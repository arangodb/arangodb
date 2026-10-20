################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule ToastTest.ResultCollectorTest do
  @moduledoc """
  Tests for ResultCollector via the pure State module.

  All state transformations are tested through State.apply_event/2 and
  State.to_test_data/1 with deterministic timestamps — no GenServer
  callback faking, no Process.sleep for timestamp ordering.
  """

  use ExUnit.Case, async: true

  alias ToastTest.ResultCollector.State

  import Toast.FormatterTestHelpers, only: [make_test: 0, make_test: 1]

  # Fixed timestamps for deterministic testing
  @t0 ~U[2026-01-01 00:00:00.000000Z]
  @t1 ~U[2026-01-01 00:00:01.000000Z]
  @t2 ~U[2026-01-01 00:00:02.000000Z]
  @t3 ~U[2026-01-01 00:00:03.000000Z]
  @t4 ~U[2026-01-01 00:00:04.000000Z]
  @t5 ~U[2026-01-01 00:00:05.000000Z]
  @t6 ~U[2026-01-01 00:00:06.000000Z]
  @t7 ~U[2026-01-01 00:00:07.000000Z]
  @t8 ~U[2026-01-01 00:00:08.000000Z]

  defp module_event(type, module, now) do
    {type, %ExUnit.TestModule{name: module, state: nil}, now}
  end

  # --- new/1 ---

  describe "new/1" do
    test "initial state has empty modules, module_timestamps, and test_start_times" do
      state = State.new(@t0)

      assert state.modules == %{}
      assert state.module_timestamps == %{}
      assert state.test_start_times == %{}
      assert state.failures == []
      assert state.finished_at == nil
      assert state.times_us == nil
    end

    test "config is stored" do
      state = State.new(@t0, suite: "smoke", timeout: 5000)

      assert state.config == [suite: "smoke", timeout: 5000]
    end
  end

  # --- :suite_started ---

  describe ":suite_started event" do
    test "updates suite_started_at" do
      state = State.new(@t0) |> State.apply_event({:suite_started, [], @t1})

      assert state.suite_started_at == @t1
    end
  end

  # --- unknown events ---

  describe "unknown events" do
    test "returns state unchanged" do
      state = State.new(@t0)
      assert State.apply_event(state, {:something_unexpected, "data", @t1}) == state
    end
  end

  # --- :module_started ---

  describe ":module_started event" do
    test "records module started_at in module_timestamps" do
      state = State.new(@t0) |> State.apply_event(module_event(:module_started, FakeTest, @t1))

      assert state.module_timestamps[FakeTest].started_at == @t1
    end

    test "does not overwrite existing timestamps for same module" do
      state =
        State.new(@t0)
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event(module_event(:module_started, FakeTest, @t2))

      assert state.module_timestamps[FakeTest].started_at == @t1
    end

    test "separate modules get independent timestamps" do
      state =
        State.new(@t0)
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event(module_event(:module_started, OtherTest, @t2))

      assert state.module_timestamps[FakeTest].started_at == @t1
      assert state.module_timestamps[OtherTest].started_at == @t2
    end
  end

  # --- :module_finished ---

  describe ":module_finished event" do
    test "records module finished_at in module_timestamps" do
      state =
        State.new(@t0)
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event(module_event(:module_finished, FakeTest, @t2))

      assert state.module_timestamps[FakeTest].finished_at == @t2
    end
  end

  # --- :test_started ---

  describe ":test_started event" do
    test "records test start time" do
      test = make_test()

      state = State.new(@t0) |> State.apply_event({:test_started, test, @t1})

      assert state.test_start_times[{test.module, test.name}] == @t1
    end

    test "first test_started for a module sets setup_finished_at" do
      test1 = make_test(%{name: :"test first"})
      test2 = make_test(%{name: :"test second"})

      state =
        State.new(@t0)
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test1, @t2})

      assert state.module_timestamps[FakeTest].setup_finished_at == @t2

      # Second test does not change setup_finished_at
      state = State.apply_event(state, {:test_started, test2, @t3})
      assert state.module_timestamps[FakeTest].setup_finished_at == @t2
    end
  end

  # --- :test_finished — outcome types ---

  describe ":test_finished — passed test" do
    test "produces a plain map with name as atom, outcome :passed, duration, timestamps, tags" do
      test = make_test()

      state =
        State.new(@t0)
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test, @t2})
        |> State.apply_event({:test_finished, test, @t3})

      assert [result] = state.modules[FakeTest]
      assert is_map(result)
      refute Map.has_key?(result, :__struct__)
      assert result.name == :"test something"
      assert result.outcome == :passed
      assert result.duration_us == 25_000
      assert result.started_at == @t2
      assert result.finished_at == DateTime.add(@t2, 25_000, :microsecond)
      assert result.tags == %{file: "test/fake_test.exs", line: 5}
    end
  end

  describe ":test_finished — failed test" do
    test "produces a plain map with outcome :failed, no failure details in test result" do
      test =
        make_test(%{
          name: :"test fails",
          state:
            {:failed, [{:error, %ExUnit.AssertionError{message: "Expected true, got false"}, []}]},
          time: 100_000
        })

      state =
        State.new(@t0)
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test, @t2})
        |> State.apply_event({:test_finished, test, @t3})

      assert [result] = state.modules[FakeTest]
      assert result.outcome == :failed
      refute Map.has_key?(result, :failure)
    end
  end

  describe ":test_finished — skipped test" do
    test "outcome is :skipped" do
      test = make_test(%{state: {:skipped, "not implemented yet"}})

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test, @t1})
        |> State.apply_event({:test_finished, test, @t2})

      assert [%{outcome: :skipped}] = state.modules[FakeTest]
    end
  end

  describe ":test_finished — between_tests_finished_at default" do
    test "between_tests_finished_at is nil on a freshly-finished test" do
      test = make_test()

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test, @t1})
        |> State.apply_event({:test_finished, test, @t2})

      assert [%{between_tests_finished_at: nil}] = state.modules[FakeTest]
    end
  end

  describe ":between_tests_finished event" do
    test "sets between_tests_finished_at on the matching test record" do
      test = make_test()

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test, @t1})
        |> State.apply_event({:test_finished, test, @t2})
        |> State.apply_event({:between_tests_finished, test, @t3})

      assert [%{between_tests_finished_at: @t3}] = state.modules[FakeTest]
    end

    test "updates only the matching test, not earlier tests in the module" do
      test1 = make_test(%{name: :"test one"})
      test2 = make_test(%{name: :"test two"})

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test1, @t1})
        |> State.apply_event({:test_finished, test1, @t2})
        |> State.apply_event({:between_tests_finished, test1, @t3})
        |> State.apply_event({:test_started, test2, @t4})
        |> State.apply_event({:test_finished, test2, @t5})
        |> State.apply_event({:between_tests_finished, test2, @t6})

      tests = state.modules[FakeTest]
      assert Enum.find(tests, &(&1.name == :"test one")).between_tests_finished_at == @t3
      assert Enum.find(tests, &(&1.name == :"test two")).between_tests_finished_at == @t6
    end

    test "is a no-op for unknown modules" do
      test = make_test()
      state = State.new(@t0) |> State.apply_event({:between_tests_finished, test, @t1})

      assert state.modules == %{}
    end

    test "is a no-op for unknown tests in a known module" do
      test1 = make_test(%{name: :"test one"})
      test2 = make_test(%{name: :"test two"})

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test1, @t1})
        |> State.apply_event({:test_finished, test1, @t2})
        |> State.apply_event({:between_tests_finished, test2, @t3})

      assert [%{between_tests_finished_at: nil}] = state.modules[FakeTest]
    end
  end

  describe ":test_finished — excluded test" do
    test "outcome is :excluded" do
      test = make_test(%{state: {:excluded, "requires cluster"}})

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test, @t1})
        |> State.apply_event({:test_finished, test, @t2})

      assert [%{outcome: :excluded}] = state.modules[FakeTest]
    end
  end

  describe ":test_finished — invalid test" do
    test "outcome is :invalid" do
      test = make_test(%{state: {:invalid, %ExUnit.TestModule{name: FakeTest, state: nil}}})

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test, @t1})
        |> State.apply_event({:test_finished, test, @t2})

      assert [%{outcome: :invalid}] = state.modules[FakeTest]
    end
  end

  describe ":test_finished — without prior test_started" do
    test "started_at and finished_at are nil in the result" do
      test = make_test(%{name: :"test orphan", time: 50_000})

      state =
        State.new(@t0)
        |> State.apply_event({:test_finished, test, @t1})

      assert [result] = state.modules[FakeTest]
      assert result.name == :"test orphan"
      assert result.started_at == nil
      assert result.finished_at == nil
      assert result.duration_us == 50_000
    end
  end

  describe ":test_finished — failures list" do
    test "failed test is added to failures as raw ExUnit.Test struct" do
      test =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "nope"}, []}]},
          time: 100_000
        })

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test, @t1})
        |> State.apply_event({:test_finished, test, @t2})

      assert [%ExUnit.Test{name: :"test fails"}] = state.failures
    end

    test "passed test is not added to failures" do
      test = make_test()

      state =
        State.new(@t0)
        |> State.apply_event({:test_started, test, @t1})
        |> State.apply_event({:test_finished, test, @t2})

      assert state.failures == []
    end
  end

  # --- :test_finished — teardown_started_at tracking ---

  describe ":test_finished — teardown_started_at tracking" do
    test "each test_finished updates teardown_started_at" do
      test1 = make_test(%{name: :"test first", time: 10_000})
      test2 = make_test(%{name: :"test second", time: 20_000})

      state =
        State.new(@t0)
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test1, @t2})
        |> State.apply_event({:test_finished, test1, @t3})

      assert state.module_timestamps[FakeTest].teardown_started_at == @t3

      state =
        state
        |> State.apply_event({:test_started, test2, @t4})
        |> State.apply_event({:test_finished, test2, @t5})

      assert state.module_timestamps[FakeTest].teardown_started_at == @t5
    end
  end

  # --- :suite_finished + to_test_data/1 ---

  describe ":suite_finished + to_test_data/1" do
    test "produces complete data map" do
      times_us = %{async: 0, sync: 30_000}
      test1 = make_test(%{name: :"test first", time: 10_000})
      test2 = make_test(%{name: :"test second", time: 20_000})

      data =
        State.new(@t0, suite: "smoke")
        |> State.apply_event({:suite_started, [], @t1})
        |> State.apply_event(module_event(:module_started, FakeTest, @t2))
        |> State.apply_event({:test_started, test1, @t3})
        |> State.apply_event({:test_finished, test1, @t4})
        |> State.apply_event({:test_started, test2, @t5})
        |> State.apply_event({:test_finished, test2, @t6})
        |> State.apply_event(module_event(:module_finished, FakeTest, @t7))
        |> State.apply_event({:suite_finished, times_us, @t8})
        |> State.to_test_data()

      assert data.suite == "smoke"
      assert data.started_at == @t1
      assert data.finished_at == @t8
      assert data.times_us == times_us
      assert data.failures == []

      mod = data.modules[FakeTest]
      assert mod != nil
      assert mod.started_at == @t2
      assert mod.finished_at == @t7
      assert length(mod.tests) == 2
    end

    test "tests are in execution order (not reversed)" do
      test1 = make_test(%{name: :"test first", time: 10_000})
      test2 = make_test(%{name: :"test second", time: 20_000})
      test3 = make_test(%{name: :"test third", time: 30_000})

      data =
        State.new(@t0, suite: "order")
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test1, @t2})
        |> State.apply_event({:test_finished, test1, @t3})
        |> State.apply_event({:test_started, test2, @t4})
        |> State.apply_event({:test_finished, test2, @t5})
        |> State.apply_event({:test_started, test3, @t6})
        |> State.apply_event({:test_finished, test3, @t7})
        |> State.to_test_data()

      names = Enum.map(data.modules[FakeTest].tests, & &1.name)
      assert names == [:"test first", :"test second", :"test third"]
    end

    test "modules have correct timestamps from module and test events" do
      test = make_test(%{name: :"test only", time: 10_000})

      data =
        State.new(@t0, suite: "timestamps")
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test, @t2})
        |> State.apply_event({:test_finished, test, @t3})
        |> State.apply_event(module_event(:module_finished, FakeTest, @t4))
        |> State.to_test_data()

      mod = data.modules[FakeTest]

      assert mod.started_at == @t1
      assert mod.finished_at == @t4
      assert mod.setup_finished_at == @t2
      assert mod.teardown_started_at == @t3
    end

    test "suite name comes from config :suite option" do
      data =
        State.new(@t0, suite: "replication2")
        |> State.apply_event({:suite_finished, %{async: 0, sync: 0}, @t1})
        |> State.to_test_data()

      assert data.suite == "replication2"
    end

    test "suite name is nil when not configured" do
      data =
        State.new(@t0)
        |> State.apply_event({:suite_finished, %{async: 0, sync: 0}, @t1})
        |> State.to_test_data()

      assert data.suite == nil
    end
  end

  # --- build_module_result with nil timestamps ---

  describe "build_module_result with nil timestamps (no module_started)" do
    test "derives module started_at/finished_at from test times" do
      test1 = make_test(%{name: :"test alpha", time: 10_000})
      test2 = make_test(%{name: :"test beta", time: 20_000})

      # Send test_started/test_finished without module_started — the module
      # will have no entry in module_timestamps, so build_module_result
      # falls back to computing timestamps from individual test times.
      data =
        State.new(@t0, suite: "no-module-started")
        |> State.apply_event({:test_started, test1, @t1})
        |> State.apply_event({:test_finished, test1, @t2})
        |> State.apply_event({:test_started, test2, @t3})
        |> State.apply_event({:test_finished, test2, @t4})
        |> State.to_test_data()

      mod = data.modules[FakeTest]
      assert mod.started_at == @t1
      assert mod.finished_at == DateTime.add(@t3, 20_000, :microsecond)
      assert mod.setup_finished_at == nil
      assert mod.teardown_started_at == nil
      assert length(mod.tests) == 2
    end
  end

  # --- Multiple modules ---

  describe "multiple modules" do
    test "tests from different modules are correctly separated" do
      test1 = make_test(%{name: :"test alpha", time: 10_000})
      test2 = make_test(%{name: :"test beta", module: OtherTest, time: 20_000})

      data =
        State.new(@t0, suite: "multi")
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test1, @t2})
        |> State.apply_event({:test_finished, test1, @t3})
        |> State.apply_event(module_event(:module_finished, FakeTest, @t4))
        |> State.apply_event(module_event(:module_started, OtherTest, @t5))
        |> State.apply_event({:test_started, test2, @t6})
        |> State.apply_event({:test_finished, test2, @t7})
        |> State.apply_event(module_event(:module_finished, OtherTest, @t8))
        |> State.to_test_data()

      assert map_size(data.modules) == 2
      assert [%{name: :"test alpha"}] = data.modules[FakeTest].tests
      assert [%{name: :"test beta"}] = data.modules[OtherTest].tests
    end

    test "each module has its own timestamps" do
      test1 = make_test(%{name: :"test one", time: 10_000})
      test2 = make_test(%{name: :"test two", module: OtherTest, time: 20_000})

      data =
        State.new(@t0, suite: "multi-ts")
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test1, @t2})
        |> State.apply_event({:test_finished, test1, @t3})
        |> State.apply_event(module_event(:module_finished, FakeTest, @t4))
        |> State.apply_event(module_event(:module_started, OtherTest, @t5))
        |> State.apply_event({:test_started, test2, @t6})
        |> State.apply_event({:test_finished, test2, @t7})
        |> State.apply_event(module_event(:module_finished, OtherTest, @t8))
        |> State.to_test_data()

      fake = data.modules[FakeTest]
      other = data.modules[OtherTest]

      assert fake.started_at == @t1
      assert other.started_at == @t5
      assert fake.setup_finished_at == @t2
      assert other.setup_finished_at == @t6
      assert fake.teardown_started_at == @t3
      assert other.teardown_started_at == @t7
    end
  end

  # --- Module timestamps edge cases ---

  describe "module timestamps edge cases" do
    test "module with no tests: setup_finished_at and teardown_started_at are nil" do
      data =
        State.new(@t0, suite: "empty-mod")
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event(module_event(:module_finished, FakeTest, @t2))
        |> State.to_test_data()

      mod = data.modules[FakeTest]

      assert mod.started_at == @t1
      assert mod.finished_at == @t2
      assert mod.setup_finished_at == nil
      assert mod.teardown_started_at == nil
      assert mod.tests == []
    end

    test "module with one test: setup_finished_at and teardown_started_at are both set" do
      test = make_test(%{name: :"test only", time: 10_000})

      data =
        State.new(@t0, suite: "single-test")
        |> State.apply_event(module_event(:module_started, FakeTest, @t1))
        |> State.apply_event({:test_started, test, @t2})
        |> State.apply_event({:test_finished, test, @t3})
        |> State.apply_event(module_event(:module_finished, FakeTest, @t4))
        |> State.to_test_data()

      mod = data.modules[FakeTest]

      assert mod.setup_finished_at == @t2
      assert mod.teardown_started_at == @t3
    end
  end

  # --- Failures tracking ---

  describe "failures tracking" do
    test "only failed tests appear in failures list" do
      passed = make_test(%{name: :"test passes", time: 10_000})
      skipped = make_test(%{name: :"test skipped", state: {:skipped, "skip"}, time: 0})

      failed =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "nope"}, []}]},
          time: 50_000
        })

      excluded = make_test(%{name: :"test excluded", state: {:excluded, "no"}, time: 0})

      data =
        State.new(@t0, suite: "failures")
        |> State.apply_event({:test_started, passed, @t1})
        |> State.apply_event({:test_finished, passed, @t2})
        |> State.apply_event({:test_started, skipped, @t3})
        |> State.apply_event({:test_finished, skipped, @t4})
        |> State.apply_event({:test_started, failed, @t5})
        |> State.apply_event({:test_finished, failed, @t6})
        |> State.apply_event({:test_started, excluded, @t7})
        |> State.apply_event({:test_finished, excluded, @t8})
        |> State.to_test_data()

      assert length(data.failures) == 1
      assert hd(data.failures).name == :"test fails"
    end

    test "failures are in execution order" do
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

      data =
        State.new(@t0, suite: "fail-order")
        |> State.apply_event({:test_started, fail1, @t1})
        |> State.apply_event({:test_finished, fail1, @t2})
        |> State.apply_event({:test_started, fail2, @t3})
        |> State.apply_event({:test_finished, fail2, @t4})
        |> State.to_test_data()

      failure_names = Enum.map(data.failures, & &1.name)
      assert failure_names == [:"test fail first", :"test fail second"]
    end

    test "failures contain raw ExUnit.Test structs" do
      failed =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "boom"}, []}]},
          time: 100_000
        })

      data =
        State.new(@t0, suite: "raw-failures")
        |> State.apply_event({:test_started, failed, @t1})
        |> State.apply_event({:test_finished, failed, @t2})
        |> State.to_test_data()

      assert [%ExUnit.Test{} = failure] = data.failures
      assert failure.name == :"test fails"
      assert {:failed, _} = failure.state
    end
  end

  # --- GenServer integration ---

  describe "GenServer integration" do
    test "processes a cast event and returns it via get_data" do
      test = make_test(%{name: :"test via genserver", time: 15_000})

      {:ok, pid} = GenServer.start_link(ToastTest.ResultCollector, suite: "integration")

      GenServer.cast(pid, {:test_started, test})
      GenServer.cast(pid, {:test_finished, test})

      data = ToastTest.ResultCollector.get_data(pid)

      assert data.suite == "integration"
      mod = data.modules[FakeTest]
      assert mod != nil
      assert [result] = mod.tests
      assert result.name == :"test via genserver"
      assert result.outcome == :passed
      assert result.duration_us == 15_000
      # The GenServer wraps with DateTime.utc_now(), so timestamps are real
      assert %DateTime{} = result.started_at
      assert %DateTime{} = result.finished_at

      GenServer.stop(pid)
    end
  end
end
