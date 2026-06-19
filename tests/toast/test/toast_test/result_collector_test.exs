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

  The collector tracks outcomes, durations, and failures only. It carries no
  timestamps — every time window (suite, module, test) is derived from the
  event stream by `TimeWindows`, so `apply_event/2` takes plain
  `{event, payload}` tuples with no clock.
  """

  use ExUnit.Case, async: true

  alias ToastTest.ResultCollector.State

  import Toast.FormatterTestHelpers, only: [make_test: 0, make_test: 1]

  defp module_event(type, module) do
    {type, %ExUnit.TestModule{name: module, state: nil}}
  end

  # --- new/1 ---

  describe "new/1" do
    test "initial state is empty" do
      state = State.new()

      assert state.modules == %{}
      assert state.failures == []
      assert state.times_us == nil
    end

    test "config is stored" do
      state = State.new(suite: "smoke", timeout: 5000)

      assert state.config == [suite: "smoke", timeout: 5000]
    end
  end

  # --- no-op events ---

  describe "events not tracked by the collector" do
    test "unknown events leave state unchanged" do
      state = State.new()
      assert State.apply_event(state, {:something_unexpected, "data"}) == state
    end

    test ":suite_started, :test_started, :module_finished, :between_tests_finished are no-ops" do
      test = make_test()
      base = State.new() |> State.apply_event(module_event(:module_started, FakeTest))

      assert State.apply_event(base, {:suite_started, []}) == base
      assert State.apply_event(base, {:test_started, test}) == base
      assert State.apply_event(base, module_event(:module_finished, FakeTest)) == base
      assert State.apply_event(base, {:between_tests_finished, test}) == base
    end
  end

  # --- :module_started ---

  describe ":module_started event" do
    test "registers the module so empty modules still appear" do
      state = State.new() |> State.apply_event(module_event(:module_started, FakeTest))

      assert state.modules == %{FakeTest => []}
    end

    test "does not clobber tests already recorded for the module" do
      test = make_test()

      state =
        State.new()
        |> State.apply_event({:test_finished, test})
        |> State.apply_event(module_event(:module_started, FakeTest))

      assert [%{name: :"test something"}] = state.modules[FakeTest]
    end
  end

  # --- :test_finished — result shape ---

  describe ":test_finished — result shape" do
    test "produces a plain map with name, outcome, duration, tags and no timestamps" do
      test = make_test()

      state = State.new() |> State.apply_event({:test_finished, test})

      assert [result] = state.modules[FakeTest]
      assert is_map(result)
      refute Map.has_key?(result, :__struct__)
      assert result.name == :"test something"
      assert result.outcome == :passed
      assert result.duration_us == 25_000
      assert result.tags == %{file: "test/fake_test.exs", line: 5}
      refute Map.has_key?(result, :started_at)
      refute Map.has_key?(result, :finished_at)
    end

    test "failed test carries outcome :failed and no failure details in the result" do
      test =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "nope"}, []}]},
          time: 100_000
        })

      state = State.new() |> State.apply_event({:test_finished, test})

      assert [result] = state.modules[FakeTest]
      assert result.outcome == :failed
      refute Map.has_key?(result, :failure)
    end

    test "maps skipped, excluded, and invalid outcomes" do
      cases = [
        {{:skipped, "later"}, :skipped},
        {{:excluded, "requires cluster"}, :excluded},
        {{:invalid, %ExUnit.TestModule{name: FakeTest, state: nil}}, :invalid}
      ]

      for {state_tuple, outcome} <- cases do
        test = make_test(%{state: state_tuple})
        state = State.new() |> State.apply_event({:test_finished, test})

        assert [%{outcome: ^outcome}] = state.modules[FakeTest]
      end
    end
  end

  # --- :suite_finished + to_test_data/1 ---

  describe ":suite_finished + to_test_data/1" do
    test "produces the public data map" do
      times_us = %{async: 0, sync: 30_000}
      test1 = make_test(%{name: :"test first", time: 10_000})
      test2 = make_test(%{name: :"test second", time: 20_000})

      data =
        State.new(suite: "smoke")
        |> State.apply_event(module_event(:module_started, FakeTest))
        |> State.apply_event({:test_finished, test1})
        |> State.apply_event({:test_finished, test2})
        |> State.apply_event({:suite_finished, times_us})
        |> State.to_test_data()

      assert data.suite == "smoke"
      assert data.times_us == times_us
      assert data.failures == []
      assert %{tests: tests} = data.modules[FakeTest]
      assert length(tests) == 2
      refute Map.has_key?(data, :started_at)
      refute Map.has_key?(data, :finished_at)
    end

    test "tests are in execution order (not reversed)" do
      test1 = make_test(%{name: :"test first", time: 10_000})
      test2 = make_test(%{name: :"test second", time: 20_000})
      test3 = make_test(%{name: :"test third", time: 30_000})

      data =
        State.new(suite: "order")
        |> State.apply_event({:test_finished, test1})
        |> State.apply_event({:test_finished, test2})
        |> State.apply_event({:test_finished, test3})
        |> State.to_test_data()

      names = Enum.map(data.modules[FakeTest].tests, & &1.name)
      assert names == [:"test first", :"test second", :"test third"]
    end

    test "an empty module (started, no tests) is retained with no tests" do
      data =
        State.new(suite: "empty-mod")
        |> State.apply_event(module_event(:module_started, FakeTest))
        |> State.apply_event(module_event(:module_finished, FakeTest))
        |> State.to_test_data()

      assert data.modules[FakeTest] == %{tests: []}
    end

    test "suite name comes from config :suite option" do
      data =
        State.new(suite: "replication2")
        |> State.apply_event({:suite_finished, %{async: 0, sync: 0}})
        |> State.to_test_data()

      assert data.suite == "replication2"
    end

    test "suite name is nil when not configured" do
      data =
        State.new()
        |> State.apply_event({:suite_finished, %{async: 0, sync: 0}})
        |> State.to_test_data()

      assert data.suite == nil
    end
  end

  # --- Multiple modules ---

  describe "multiple modules" do
    test "tests from different modules are correctly separated" do
      test1 = make_test(%{name: :"test alpha", time: 10_000})
      test2 = make_test(%{name: :"test beta", module: OtherTest, time: 20_000})

      data =
        State.new(suite: "multi")
        |> State.apply_event({:test_finished, test1})
        |> State.apply_event({:test_finished, test2})
        |> State.to_test_data()

      assert map_size(data.modules) == 2
      assert [%{name: :"test alpha"}] = data.modules[FakeTest].tests
      assert [%{name: :"test beta"}] = data.modules[OtherTest].tests
    end
  end

  # --- Failures tracking ---

  describe "failures tracking" do
    test "only failed tests appear in failures list, in execution order" do
      passed = make_test(%{name: :"test passes", time: 10_000})
      skipped = make_test(%{name: :"test skipped", state: {:skipped, "skip"}, time: 0})

      fail1 =
        make_test(%{
          name: :"test fail first",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "a"}, []}]},
          time: 50_000
        })

      fail2 =
        make_test(%{
          name: :"test fail second",
          state: {:failed, [{:error, %ExUnit.AssertionError{message: "b"}, []}]},
          time: 60_000
        })

      data =
        State.new(suite: "failures")
        |> State.apply_event({:test_finished, passed})
        |> State.apply_event({:test_finished, skipped})
        |> State.apply_event({:test_finished, fail1})
        |> State.apply_event({:test_finished, fail2})
        |> State.to_test_data()

      assert Enum.map(data.failures, & &1.name) == [:"test fail first", :"test fail second"]
      assert Enum.all?(data.failures, &match?(%ExUnit.Test{state: {:failed, _}}, &1))
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
      assert [result] = data.modules[FakeTest].tests
      assert result.name == :"test via genserver"
      assert result.outcome == :passed
      assert result.duration_us == 15_000

      GenServer.stop(pid)
    end
  end
end
