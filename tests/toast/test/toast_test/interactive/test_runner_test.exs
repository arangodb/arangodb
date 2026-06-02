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

defmodule ToastTest.Interactive.TestRunnerTest do
  use ExUnit.Case, async: false

  alias ToastTest.Interactive.TestRunner

  # Fixture modules use a :test_runner_collector guard so they only
  # exhibit test behavior (raising, on_exit, etc.) when driven by
  # these tests, not when picked up by `mix test` directly.

  defmodule Fixtures.Passing do
    use ExUnit.Case, async: false
    test "passes", do: assert(1 + 1 == 2)
  end

  defmodule Fixtures.WithSetup do
    use ExUnit.Case, async: false

    setup do
      {:ok, setup_value: 42}
    end

    test("uses setup context", %{setup_value: val}, do: assert(val == 42))
  end

  defmodule Fixtures.WithSetupAll do
    use ExUnit.Case, async: false

    setup_all do
      {:ok, shared: "hello"}
    end

    test("uses setup_all context", %{shared: val}, do: assert(val == "hello"))
  end

  defmodule Fixtures.OnExitPerTest do
    use ExUnit.Case, async: false

    test "registers on_exit" do
      collector = Process.whereis(:test_runner_collector)

      if collector do
        on_exit(fn -> send(collector, {:on_exit_ran, self()}) end)
      end

      assert true
    end
  end

  defmodule Fixtures.OnExitFailure do
    use ExUnit.Case, async: false

    test "passes but on_exit fails" do
      collector = Process.whereis(:test_runner_collector)
      if collector, do: on_exit(fn -> raise "on_exit boom" end)
      assert true
    end
  end

  defmodule Fixtures.SetupAllOnExit do
    use ExUnit.Case, async: false

    setup_all do
      collector = Process.whereis(:test_runner_collector)

      if collector do
        on_exit(fn -> send(collector, {:setup_all_on_exit_ran, self()}) end)
      end

      {:ok, %{}}
    end

    test "first", do: assert(true)
    test "second", do: assert(true)
  end

  defmodule Fixtures.TwoTests do
    use ExUnit.Case, async: false
    test "alpha", do: assert(true)
    test "beta", do: assert(true)
  end

  defmodule Fixtures.ProcessId do
    use ExUnit.Case, async: false

    test "reports own pid" do
      collector = Process.whereis(:test_runner_collector)
      if collector, do: send(collector, {:test_pid, self()})
    end
  end

  defp register_collector do
    Process.register(self(), :test_runner_collector)

    on_exit(fn ->
      try do
        Process.unregister(:test_runner_collector)
      rescue
        _ -> :ok
      end
    end)
  end

  describe "run_module_tests/2 with passing tests" do
    test "returns passed outcome" do
      results = TestRunner.run_module_tests(Fixtures.Passing, nil)
      assert [%{outcome: :passed, failure: nil}] = results
    end

    test "result contains the test name as an atom" do
      [result] = TestRunner.run_module_tests(Fixtures.Passing, nil)
      assert result.name == :"test passes"
    end
  end

  describe "run_module_tests/2 with failing tests" do
    test "returns failed outcome with error details" do
      defmodule AlwaysFails do
        use ExUnit.Case, async: false

        test "boom" do
          if Process.whereis(:test_runner_collector), do: raise("intentional")
        end
      end

      register_collector()
      results = TestRunner.run_module_tests(AlwaysFails, nil)
      assert [%{outcome: :failed}] = results
      assert [%{failure: {:error, %RuntimeError{message: "intentional"}, _}}] = results
    end
  end

  describe "run_module_tests/2 with mixed results" do
    test "returns one passed and one failed" do
      defmodule MixedInline do
        use ExUnit.Case, async: false
        test "ok", do: assert(true)

        test "not ok" do
          if Process.whereis(:test_runner_collector), do: raise("nope")
        end
      end

      register_collector()
      results = TestRunner.run_module_tests(MixedInline, nil)
      outcomes = results |> Enum.map(& &1.outcome) |> Enum.sort()
      assert outcomes == [:failed, :passed]
    end
  end

  describe "setup callback" do
    test "per-test setup context is available in the test" do
      results = TestRunner.run_module_tests(Fixtures.WithSetup, nil)
      assert [%{outcome: :passed}] = results
    end
  end

  describe "setup_all callback" do
    test "setup_all context is merged into test context" do
      results = TestRunner.run_module_tests(Fixtures.WithSetupAll, nil)
      assert [%{outcome: :passed}] = results
    end

    test "failing setup_all marks all tests as failed" do
      defmodule FailingSetupAll do
        use ExUnit.Case, async: false

        setup_all do
          if Process.whereis(:test_runner_collector), do: raise("setup_all exploded")
          {:ok, %{}}
        end

        test "a", do: :ok
        test "b", do: :ok
      end

      register_collector()
      results = TestRunner.run_module_tests(FailingSetupAll, nil)
      assert length(results) == 2
      assert Enum.all?(results, &(&1.outcome == :failed))
    end
  end

  describe "on_exit handlers (per-test)" do
    test "on_exit handler runs after the test" do
      register_collector()
      TestRunner.run_module_tests(Fixtures.OnExitPerTest, nil)
      assert_received {:on_exit_ran, _pid}
    end

    test "on_exit failure turns passing test into failed" do
      register_collector()
      results = TestRunner.run_module_tests(Fixtures.OnExitFailure, nil)
      assert [%{outcome: :failed}] = results
    end
  end

  describe "on_exit handlers (setup_all)" do
    test "setup_all on_exit handlers run after all tests complete" do
      register_collector()
      TestRunner.run_module_tests(Fixtures.SetupAllOnExit, nil)
      assert_received {:setup_all_on_exit_ran, _pid}
    end
  end

  describe "test filtering" do
    test "filters to a specific test by name" do
      results = TestRunner.run_module_tests(Fixtures.TwoTests, "alpha")
      assert [%{name: :"test alpha", outcome: :passed}] = results
    end

    test "matches substring case-insensitively" do
      results = TestRunner.run_module_tests(Fixtures.TwoTests, "ALPH")
      assert [%{name: :"test alpha", outcome: :passed}] = results
    end

    test "returns empty list when filter matches nothing" do
      results = TestRunner.run_module_tests(Fixtures.TwoTests, "nonexistent")
      assert results == []
    end

    test "runs all tests when no filter given" do
      results = TestRunner.run_module_tests(Fixtures.TwoTests, nil)
      assert length(results) == 2
    end
  end

  describe "process isolation" do
    test "each test runs in a separate process from the caller" do
      register_collector()
      caller_pid = self()
      TestRunner.run_module_tests(Fixtures.ProcessId, nil)

      assert_received {:test_pid, test_pid}
      assert test_pid != caller_pid
    end
  end
end
