defmodule ToastTest.ExpectTest.Scenarios do
  @moduledoc false
  # Test module exercised via spawn_test in integration tests below.
  use ExUnit.Case, async: false
  @moduletag :expect_scenario
  import ToastTest.Expect, only: [expect: 1]

  test "only expects fail" do
    expect 1 == 2
    expect 3 == 4
  end

  test "expect and assert both fail" do
    expect 1 == 2
    expect 3 == 4
    assert 5 == 6
  end

  test "expects fail but assert passes" do
    expect 1 == 2
    assert 1 == 1
  end

  test "all pass" do
    expect true
    assert true
  end
end

defmodule ToastTest.ExpectTest do
  use ExUnit.Case, async: true

  alias ToastTest.Expect
  alias ToastTest.Runner.TestProcess
  import ToastTest.Expect, only: [expect: 1]

  setup do
    Expect.collect_failures()
    :ok
  end

  describe "record_failure/2 and collect_failures/0" do
    test "no failures returns empty list" do
      assert Expect.collect_failures() == []
    end

    test "records and collects a single failure" do
      error = %ExUnit.AssertionError{message: "boom"}
      stack = [{__MODULE__, :test, 1, [file: ~c"test.exs", line: 1]}]

      Expect.record_failure(error, stack)

      assert [{:error, ^error, ^stack}] = Expect.collect_failures()
    end

    test "preserves insertion order" do
      e1 = %ExUnit.AssertionError{message: "first"}
      e2 = %ExUnit.AssertionError{message: "second"}
      s = [{__MODULE__, :test, 1, []}]

      Expect.record_failure(e1, s)
      Expect.record_failure(e2, s)

      assert [{:error, %{message: "first"}, _}, {:error, %{message: "second"}, _}] =
               Expect.collect_failures()
    end

    test "collect clears the failures" do
      Expect.record_failure(%ExUnit.AssertionError{message: "x"}, [])

      assert [_] = Expect.collect_failures()
      assert [] = Expect.collect_failures()
    end
  end

  describe "expect/1 macro" do
    test "passing expectation does not record a failure" do
      expect 1 + 1 == 2

      assert Expect.collect_failures() == []
    end

    test "failing expectation records a failure but does not raise" do
      expect 1 + 1 == 3

      failures = Expect.collect_failures()
      assert length(failures) == 1
      assert [{:error, %ExUnit.AssertionError{}, _stack}] = failures
    end

    test "multiple failing expectations are all recorded" do
      expect false
      expect nil
      expect 1 == 2

      assert length(Expect.collect_failures()) == 3
    end

    test "mix of passing and failing expectations" do
      expect true
      expect false
      expect 1 == 1
      expect 1 == 2

      assert length(Expect.collect_failures()) == 2
    end

    test "returns the value on success" do
      assert expect(42) == 42
    end

    test "returns nil on failure" do
      assert expect(nil) == nil
    end

    test "works with pattern matching" do
      expect {:ok, _} = {:ok, 42}

      assert Expect.collect_failures() == []
    end

    test "records failure on pattern mismatch" do
      expect {:ok, _} = {:error, :boom}

      assert [{:error, %ExUnit.AssertionError{}, _}] = Expect.collect_failures()
    end

    test "works with comparison operators" do
      expect 3 > 2
      expect 1 < 0

      failures = Expect.collect_failures()
      assert length(failures) == 1
    end

    test "non-assertion errors propagate instead of being captured" do
      assert_raise KeyError, fn ->
        expect Map.fetch!(%{}, :missing)
      end

      assert Expect.collect_failures() == []
    end

    test "returns matched value on pattern match success" do
      assert {:ok, 42} = expect({:ok, _} = {:ok, 42})
    end
  end

  describe "runner integration" do
    setup do
      ToastTest.Abort.clear!()
      on_exit(fn -> ToastTest.Abort.clear!() end)
      :ok
    end

    defp run_scenario(test_name) do
      scenario_module = ToastTest.ExpectTest.Scenarios
      test_meta = ToastTest.ExUnitCompat.get_test_metadata(scenario_module)
      test = Enum.find(test_meta.tests, &(&1.name == :"test #{test_name}"))

      config = %{
        capture_log: false,
        timeout_settings: %ToastTest.Runner.Timeout.Settings{
          base_timeout: 5_000,
          timeout_factor: 1,
          suite_deadline: nil,
          global_deadline: nil,
          disable_timeouts: false
        }
      }

      TestProcess.spawn_test(config, test, %{})
    end

    test "only expect failures → MultiError with all failures" do
      result = run_scenario("only expects fail")

      assert {:failed, errors} = result.state
      assert length(errors) == 2
    end

    test "expect and assert both fail → all three reported" do
      result = run_scenario("expect and assert both fail")

      assert {:failed, errors} = result.state
      assert length(errors) == 3
    end

    test "expects fail but assert passes → MultiError with expect failures" do
      result = run_scenario("expects fail but assert passes")

      assert {:failed, errors} = result.state
      assert length(errors) == 1
    end

    test "all pass → no failure state" do
      result = run_scenario("all pass")

      assert result.state == nil
    end
  end
end
