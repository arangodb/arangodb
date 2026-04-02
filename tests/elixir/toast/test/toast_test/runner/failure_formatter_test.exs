defmodule ToastTest.Runner.FailureFormatterTest do
  use ExUnit.Case, async: true

  alias ToastTest.Runner.FailureFormatter

  describe "prune_stacktrace/1" do
    test "returns empty list for empty stacktrace" do
      assert FailureFormatter.prune_stacktrace([]) == []
    end

    test "keeps user test module frames" do
      stack = [
        {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]},
        {MyApp.Other, :call, 2, [file: ~c"lib/other.ex", line: 5]}
      ]

      assert FailureFormatter.prune_stacktrace(stack) == stack
    end

    test "strips ExUnit.Assertions frames from the top" do
      user_frame = {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}

      stack = [
        {ExUnit.Assertions, :assert, 1, [file: ~c"lib/ex_unit/assertions.ex", line: 100]},
        user_frame
      ]

      assert FailureFormatter.prune_stacktrace(stack) == [user_frame]
    end

    test "strips consecutive ExUnit.Assertions frames" do
      user_frame = {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}

      stack = [
        {ExUnit.Assertions, :assert, 1, [file: ~c"lib/ex_unit/assertions.ex", line: 100]},
        {ExUnit.Assertions, :assert_equal, 2, [file: ~c"lib/ex_unit/assertions.ex", line: 200]},
        user_frame
      ]

      assert FailureFormatter.prune_stacktrace(stack) == [user_frame]
    end

    test "truncates at ExUnit.Runner frame" do
      stack = [
        {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]},
        {ExUnit.Runner, :exec_test, 1, [file: ~c"lib/ex_unit/runner.ex", line: 300]}
      ]

      assert FailureFormatter.prune_stacktrace(stack) == [
               {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}
             ]
    end

    test "truncates at ToastTest.Runner frame" do
      user_frame = {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}

      stack = [
        user_frame,
        {ToastTest.Runner, :run, 1, [file: ~c"lib/runner.ex", line: 50]}
      ]

      assert FailureFormatter.prune_stacktrace(stack) == [user_frame]
    end

    test "truncates at ToastTest.Runner.TestExecution frame" do
      user_frame = {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}

      stack = [
        user_frame,
        {ToastTest.Runner.TestExecution, :execute, 2, [file: ~c"lib/test_execution.ex", line: 20]}
      ]

      assert FailureFormatter.prune_stacktrace(stack) == [user_frame]
    end

    test "truncates at ToastTest.Runner.TestProcess frame" do
      user_frame = {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}

      stack = [
        user_frame,
        {ToastTest.Runner.TestProcess, :init, 1, [file: ~c"lib/test_process.ex", line: 30]}
      ]

      assert FailureFormatter.prune_stacktrace(stack) == [user_frame]
    end

    test "handles mixed user frames, assertions, and runner frames" do
      stack = [
        {ExUnit.Assertions, :assert, 1, [file: ~c"lib/ex_unit/assertions.ex", line: 100]},
        {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]},
        {MyApp.Helper, :do_stuff, 0, [file: ~c"test/helper.ex", line: 5]},
        {ToastTest.Runner, :run, 1, [file: ~c"lib/runner.ex", line: 50]},
        {ShouldNotAppear, :ignored, 0, []}
      ]

      assert FailureFormatter.prune_stacktrace(stack) == [
               {MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]},
               {MyApp.Helper, :do_stuff, 0, [file: ~c"test/helper.ex", line: 5]}
             ]
    end

    test "returns empty list when all frames are internal" do
      stack = [
        {ExUnit.Assertions, :assert, 1, [file: ~c"lib/ex_unit/assertions.ex", line: 100]},
        {ToastTest.Runner, :run, 1, [file: ~c"lib/runner.ex", line: 50]}
      ]

      assert FailureFormatter.prune_stacktrace(stack) == []
    end
  end

  describe "failed/3" do
    test "wraps :error with exception in normalized failure tuple" do
      reason = %RuntimeError{message: "boom"}
      stack = [{MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}]

      assert {:failed, [{:error, normalized, ^stack}]} =
               FailureFormatter.failed(:error, reason, stack)

      assert %RuntimeError{message: "boom"} = normalized
    end

    test "normalizes raw :error atom into proper exception struct" do
      stack = [{MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}]

      assert {:failed, [{:error, normalized, ^stack}]} =
               FailureFormatter.failed(:error, :badarg, stack)

      # Exception.normalize(:error, :badarg, stack) produces %ArgumentError{}
      assert %ArgumentError{} = normalized
    end

    test "wraps :exit reason in normalized failure tuple" do
      stack = [{MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}]

      assert {:failed, [{:exit, reason, ^stack}]} =
               FailureFormatter.failed(:exit, :normal, stack)

      assert reason == :normal
    end

    test "wraps :throw value in normalized failure tuple" do
      stack = [{MyApp.SomeTest, :test_something, 1, [file: ~c"test/some_test.exs", line: 10]}]

      assert {:failed, [{:throw, value, ^stack}]} =
               FailureFormatter.failed(:throw, {:custom, :value}, stack)

      assert value == {:custom, :value}
    end

    test "failed/3 does not prune stacktrace for non-MultiError" do
      runner_frame = {ToastTest.Runner, :run, 1, [file: ~c"lib/runner.ex", line: 50]}
      stack = [runner_frame]

      assert {:failed, [{:error, _normalized, ^stack}]} =
               FailureFormatter.failed(:error, %RuntimeError{message: "boom"}, stack)
    end

    test "handles ExUnit.MultiError by expanding into multiple errors" do
      stack1 = [{MyApp.SomeTest, :test_a, 1, [file: ~c"test/some_test.exs", line: 10]}]
      stack2 = [{MyApp.SomeTest, :test_b, 1, [file: ~c"test/some_test.exs", line: 20]}]

      multi_error = %ExUnit.MultiError{
        errors: [
          {:error, %RuntimeError{message: "first"}, stack1},
          {:error, %ArgumentError{message: "second"}, stack2}
        ]
      }

      assert {:failed, errors} = FailureFormatter.failed(:error, multi_error, [])
      assert length(errors) == 2

      assert [{:error, first_normalized, first_stack}, {:error, second_normalized, second_stack}] =
               errors

      assert %RuntimeError{message: "first"} = first_normalized
      assert %ArgumentError{message: "second"} = second_normalized
      assert first_stack == stack1
      assert second_stack == stack2
    end

    test "MultiError prunes stacktraces for each individual error" do
      assertion_frame =
        {ExUnit.Assertions, :assert, 1, [file: ~c"lib/ex_unit/assertions.ex", line: 100]}

      user_frame = {MyApp.SomeTest, :test_a, 1, [file: ~c"test/some_test.exs", line: 10]}
      runner_frame = {ToastTest.Runner, :run, 1, [file: ~c"lib/runner.ex", line: 50]}

      inner_stack = [assertion_frame, user_frame, runner_frame]

      multi_error = %ExUnit.MultiError{
        errors: [{:error, %RuntimeError{message: "boom"}, inner_stack}]
      }

      assert {:failed, [{:error, _normalized, pruned_stack}]} =
               FailureFormatter.failed(:error, multi_error, [])

      assert pruned_stack == [user_frame]
    end

    test "always returns a {:failed, list()} tuple" do
      assert {:failed, errors} = FailureFormatter.failed(:error, %RuntimeError{}, [])
      assert is_list(errors)

      assert {:failed, errors} = FailureFormatter.failed(:exit, :shutdown, [])
      assert is_list(errors)

      assert {:failed, errors} = FailureFormatter.failed(:throw, "value", [])
      assert is_list(errors)
    end
  end
end
