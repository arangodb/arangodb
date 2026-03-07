defmodule ToastTest.ResultFormatterTest do
  use ExUnit.Case, async: false

  alias ToastTest.ResultFormatter

  import Toast.FormatterTestHelpers, only: [make_test: 0, make_test: 1]

  describe "init/1" do
    test "returns initial state with empty modules map" do
      {:ok, state} = ResultFormatter.init([])

      assert state.modules == %{}
      assert %DateTime{} = state.suite_started_at
      assert state.config == []
    end
  end

  describe "handle_cast {:suite_started, _}" do
    test "updates suite_started_at timestamp" do
      {:ok, state} = ResultFormatter.init([])
      early = state.suite_started_at

      # Small sleep to ensure timestamps differ
      Process.sleep(10)
      {:noreply, new_state} = ResultFormatter.handle_cast({:suite_started, []}, state)

      assert DateTime.compare(new_state.suite_started_at, early) == :gt
    end
  end

  describe "handle_cast {:test_finished, _} — passed test" do
    test "collects a passed test result" do
      {:ok, state} = ResultFormatter.init([])
      test = make_test()

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.module == FakeTest
      assert result.name == "test something"
      assert result.outcome == :passed
      assert result.duration_us == 25_000
      assert result.failure == nil
      assert result.tags == %{file: "test/fake_test.exs", line: 5}
    end
  end

  describe "handle_cast {:test_finished, _} — failed test" do
    test "collects failure info with AssertionError" do
      {:ok, state} = ResultFormatter.init([])

      error = %ExUnit.AssertionError{message: "Expected true, got false"}
      stacktrace = [{FakeTest, :"test fails", 1, [file: ~c"test/fake_test.exs", line: 10]}]

      test =
        make_test(%{
          name: :"test fails",
          state: {:failed, [{:error, error, stacktrace}]},
          time: 100_000,
          tags: %{file: "test/fake_test.exs", line: 8, test_type: :test}
        })

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :failed
      assert [failure] = result.failure
      assert failure.kind == "ExUnit.AssertionError"
      assert failure.message =~ "Expected true, got false"
      assert is_binary(failure.stacktrace)
      assert failure.stacktrace =~ "fake_test.exs"
    end
  end

  describe "handle_cast {:test_finished, _} — skipped test" do
    test "collects a skipped test result" do
      {:ok, state} = ResultFormatter.init([])
      test = make_test(%{state: {:skipped, "not implemented yet"}})

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :skipped
      assert result.failure == %{message: "not implemented yet"}
    end
  end

  describe "handle_cast {:test_finished, _} — excluded test" do
    test "collects an excluded test result" do
      {:ok, state} = ResultFormatter.init([])
      test = make_test(%{state: {:excluded, "requires cluster"}})

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :excluded
      assert result.failure == %{message: "requires cluster"}
    end
  end

  describe "handle_cast {:test_finished, _} — invalid test" do
    test "collects an invalid test result from setup_all failure" do
      {:ok, state} = ResultFormatter.init([])
      test = make_test(%{state: {:invalid, %ExUnit.TestModule{name: FakeTest, state: nil}}})

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :invalid
      assert result.failure == nil
    end
  end

  describe "handle_cast {:suite_finished, _}" do
    test "stores results in GenServer state" do
      {:ok, state} = ResultFormatter.init([])
      test = make_test()
      {:noreply, state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      times_us = %{async: 0, sync: 25_000}
      {:noreply, state} = ResultFormatter.handle_cast({:suite_finished, times_us}, state)

      {:reply, results, _state} = ResultFormatter.handle_call(:get_results, self(), state)
      assert results != nil
      assert %DateTime{} = results.started_at
      assert %DateTime{} = results.finished_at
      assert results.times_us == times_us
      assert %{FakeTest => %{tests: [result]}} = results.modules
      assert result.name == "test something"
    end
  end

  describe "multiple tests collected in order" do
    test "tests are returned in the order they were received" do
      {:ok, state} = ResultFormatter.init([])

      test1 = make_test(%{name: :"test first", time: 10_000})
      test2 = make_test(%{name: :"test second", time: 20_000})
      test3 = make_test(%{name: :"test third", time: 30_000, module: OtherTest})

      {:noreply, state} = ResultFormatter.handle_cast({:test_finished, test1}, state)
      {:noreply, state} = ResultFormatter.handle_cast({:test_finished, test2}, state)
      {:noreply, state} = ResultFormatter.handle_cast({:test_finished, test3}, state)

      {:noreply, state} =
        ResultFormatter.handle_cast({:suite_finished, %{async: 0, sync: 60_000}}, state)

      {:reply, results, _state} = ResultFormatter.handle_call(:get_results, self(), state)

      fake_names = Enum.map(results.modules[FakeTest].tests, & &1.name)
      assert fake_names == ["test first", "test second"]

      other_names = Enum.map(results.modules[OtherTest].tests, & &1.name)
      assert other_names == ["test third"]
    end
  end

  describe "failure extraction for exit and throw" do
    test "exit failure is extracted correctly" do
      {:ok, state} = ResultFormatter.init([])
      stacktrace = [{FakeTest, :"test exits", 1, [file: ~c"test/fake_test.exs", line: 15]}]

      test =
        make_test(%{
          name: :"test exits",
          state: {:failed, [{:exit, :killed, stacktrace}]},
          time: 50_000
        })

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :failed
      assert [failure] = result.failure
      assert failure.kind == "exit"
      assert failure.message == ":killed"
      assert is_binary(failure.stacktrace)
    end

    test "throw failure is extracted correctly" do
      {:ok, state} = ResultFormatter.init([])
      stacktrace = [{FakeTest, :"test throws", 1, [file: ~c"test/fake_test.exs", line: 20]}]

      test =
        make_test(%{
          name: :"test throws",
          state: {:failed, [{:throw, "something went wrong", stacktrace}]},
          time: 75_000
        })

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :failed
      assert [failure] = result.failure
      assert failure.kind == "throw"
      assert failure.message == "\"something went wrong\""
      assert is_binary(failure.stacktrace)
    end

    test "erlang error (non-struct) failure is extracted correctly" do
      {:ok, state} = ResultFormatter.init([])
      stacktrace = [{FakeTest, :"test crashes", 1, [file: ~c"test/fake_test.exs", line: 25]}]

      test =
        make_test(%{
          name: :"test crashes",
          state: {:failed, [{:error, :badarg, stacktrace}]},
          time: 10_000
        })

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :failed
      assert [failure] = result.failure
      assert failure.kind == "ErlangError"
      assert failure.message == ":badarg"
      assert is_binary(failure.stacktrace)
    end

    test "server crash EXIT failure is captured as linked process exit" do
      {:ok, state} = ResultFormatter.init([])
      pid = spawn(fn -> :ok end)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 134,
        signal: 6,
        timestamp: DateTime.utc_now()
      }

      test =
        make_test(%{
          name: :"test server crash",
          state: {:failed, [{{:EXIT, pid}, {:server_crashed, "srv-1", crash_info}, []}]},
          time: 5_000
        })

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :failed
      assert [failure] = result.failure
      assert failure.kind == "EXIT"
      assert failure.message =~ "server_crashed"
    end

    test "generic linked process EXIT failure is extracted correctly" do
      {:ok, state} = ResultFormatter.init([])
      pid = spawn(fn -> :ok end)

      test =
        make_test(%{
          name: :"test linked crash",
          state: {:failed, [{{:EXIT, pid}, :some_reason, []}]},
          time: 5_000
        })

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_finished, test}, state)

      assert [result] = new_state.modules[FakeTest]
      assert result.outcome == :failed
      assert [failure] = result.failure
      assert failure.kind == "EXIT"
      assert failure.message =~ "linked process"
      assert is_binary(failure.stacktrace)
    end
  end

  describe "failures in results" do
    test "results include raw ExUnit.Test structs for failed tests" do
      {:ok, state} = ResultFormatter.init([])

      passed = make_test(%{name: :"test passes", time: 10_000})
      failed = make_test(%{name: :"test fails", state: {:failed, []}, time: 100_000})

      {:noreply, state} = ResultFormatter.handle_cast({:test_finished, passed}, state)
      {:noreply, state} = ResultFormatter.handle_cast({:test_finished, failed}, state)

      {:noreply, state} =
        ResultFormatter.handle_cast({:suite_finished, %{async: 0, sync: 110_000}}, state)

      {:reply, results, _} = ResultFormatter.handle_call(:get_results, self(), state)
      assert length(results.failures) == 1
      assert hd(results.failures).name == :"test fails"
    end

    test "results have empty failures when no tests failed" do
      {:ok, state} = ResultFormatter.init([])
      passed = make_test(%{name: :"test passes", time: 10_000})

      {:noreply, state} = ResultFormatter.handle_cast({:test_finished, passed}, state)

      {:noreply, state} =
        ResultFormatter.handle_cast({:suite_finished, %{async: 0, sync: 10_000}}, state)

      {:reply, results, _} = ResultFormatter.handle_call(:get_results, self(), state)
      assert results.failures == []
    end
  end

  describe "handle_cast with unhandled messages" do
    test "ignores unknown messages" do
      {:ok, state} = ResultFormatter.init([])

      {:noreply, new_state} = ResultFormatter.handle_cast({:test_started, %{}}, state)
      assert new_state == state

      {:noreply, new_state} = ResultFormatter.handle_cast({:module_started, %{}}, state)
      assert new_state == state

      {:noreply, new_state} =
        ResultFormatter.handle_cast(
          {:module_finished, %ExUnit.TestModule{name: FakeTest, state: nil}},
          state
        )

      assert new_state == state
    end
  end
end
