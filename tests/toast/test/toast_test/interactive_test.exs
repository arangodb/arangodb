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

defmodule ToastTest.InteractiveTest do
  use ExUnit.Case, async: false

  import ExUnit.CaptureIO

  import Toast.DeploymentTestHelpers, only: [setup_deployment_registry: 1]

  alias ToastTest.DeploymentRegistry
  alias ToastTest.Interactive

  # -- Fixture modules --
  #
  # These `use ExUnit.Case` so Interactive.run/2 can call __ex_unit__/0 on them.
  # They are also picked up by `mix test`, so all behavior that should only
  # happen when run via Interactive is guarded by checking whether
  # :interactive_test_collector is registered (a proxy for "run via our tests").

  defmodule Fixtures.Passing do
    use ExUnit.Case, async: false

    test "passes" do
      assert 1 + 1 == 2
    end
  end

  defmodule Fixtures.WithSetup do
    use ExUnit.Case, async: false

    setup do
      {:ok, setup_value: 42}
    end

    test "uses setup context", %{setup_value: val} do
      assert val == 42
    end
  end

  defmodule Fixtures.WithSetupAll do
    use ExUnit.Case, async: false

    setup_all do
      {:ok, shared: "hello"}
    end

    test "uses setup_all context", %{shared: val} do
      assert val == "hello"
    end
  end

  defmodule Fixtures.OnExitPerTest do
    use ExUnit.Case, async: false

    test "registers on_exit" do
      collector = Process.whereis(:interactive_test_collector)

      if collector do
        on_exit(fn ->
          send(collector, {:on_exit_ran, self()})
        end)
      end

      assert true
    end
  end

  defmodule Fixtures.OnExitFailure do
    use ExUnit.Case, async: false

    test "passes but on_exit fails" do
      collector = Process.whereis(:interactive_test_collector)

      if collector do
        on_exit(fn -> raise "on_exit boom" end)
      end

      assert true
    end
  end

  defmodule Fixtures.SetupAllOnExit do
    use ExUnit.Case, async: false

    setup_all do
      collector = Process.whereis(:interactive_test_collector)

      if collector do
        on_exit(fn ->
          send(collector, {:setup_all_on_exit_ran, self()})
        end)
      end

      {:ok, %{}}
    end

    test "first" do
      assert true
    end

    test "second" do
      assert true
    end
  end

  defmodule Fixtures.TwoTests do
    use ExUnit.Case, async: false

    test "alpha" do
      assert true
    end

    test "beta" do
      assert true
    end
  end

  defmodule Fixtures.ProcessId do
    use ExUnit.Case, async: false

    test "reports own pid" do
      collector = Process.whereis(:interactive_test_collector)
      if collector, do: send(collector, {:test_pid, self()})
    end
  end

  # -- Setup --

  setup :setup_deployment_registry

  defp register_collector do
    Process.register(self(), :interactive_test_collector)

    on_exit(fn ->
      try do
        Process.unregister(:interactive_test_collector)
      rescue
        _ -> :ok
      end
    end)
  end

  # -- Tests --

  describe "run/2 with passing tests" do
    test "returns passed outcome" do
      results = Interactive.run(Fixtures.Passing, deployment: :fake)
      assert [%{outcome: :passed, failure: nil}] = results
    end

    test "result contains the test name as an atom" do
      [result] = Interactive.run(Fixtures.Passing, deployment: :fake)
      assert result.name == :"test passes"
    end
  end

  describe "run/2 with failing tests" do
    test "returns failed outcome with error details" do
      defmodule AlwaysFails do
        use ExUnit.Case, async: false

        # Guarded so mix test doesn't report a failure for the fixture itself.
        test "boom" do
          if Process.whereis(:interactive_test_collector),
            do: raise("intentional")
        end
      end

      register_collector()
      results = Interactive.run(AlwaysFails, deployment: :fake)
      assert [%{outcome: :failed}] = results
      assert [%{failure: {:error, %RuntimeError{message: "intentional"}, _}}] = results
    end
  end

  describe "run/2 with mixed results" do
    test "returns one passed and one failed" do
      defmodule MixedInline do
        use ExUnit.Case, async: false

        test "ok" do
          assert true
        end

        test "not ok" do
          if Process.whereis(:interactive_test_collector),
            do: raise("nope")
        end
      end

      register_collector()
      results = Interactive.run(MixedInline, deployment: :fake)
      outcomes = results |> Enum.map(& &1.outcome) |> Enum.sort()
      assert outcomes == [:failed, :passed]
    end
  end

  describe "setup callback" do
    test "per-test setup context is available in the test" do
      results = Interactive.run(Fixtures.WithSetup, deployment: :fake)
      assert [%{outcome: :passed}] = results
    end
  end

  describe "setup_all callback" do
    test "setup_all context is merged into test context" do
      results = Interactive.run(Fixtures.WithSetupAll, deployment: :fake)
      assert [%{outcome: :passed}] = results
    end

    test "failing setup_all marks all tests as failed" do
      defmodule FailingSetupAll do
        use ExUnit.Case, async: false

        setup_all do
          if Process.whereis(:interactive_test_collector),
            do: raise("setup_all exploded")

          {:ok, %{}}
        end

        test "a", do: :ok
        test "b", do: :ok
      end

      register_collector()
      results = Interactive.run(FailingSetupAll, deployment: :fake)
      assert length(results) == 2
      assert Enum.all?(results, &(&1.outcome == :failed))
    end
  end

  describe "on_exit handlers (per-test)" do
    test "on_exit handler runs after the test" do
      register_collector()
      Interactive.run(Fixtures.OnExitPerTest, deployment: :fake)
      assert_received {:on_exit_ran, _pid}
    end

    test "on_exit failure turns passing test into failed" do
      register_collector()
      results = Interactive.run(Fixtures.OnExitFailure, deployment: :fake)
      assert [%{outcome: :failed}] = results
    end
  end

  describe "on_exit handlers (setup_all)" do
    test "setup_all on_exit handlers run after all tests complete" do
      register_collector()
      Interactive.run(Fixtures.SetupAllOnExit, deployment: :fake)
      assert_received {:setup_all_on_exit_ran, _pid}
    end
  end

  describe "test filtering" do
    test "filters to a specific test by name" do
      results = Interactive.run(Fixtures.TwoTests, deployment: :fake, test: "alpha")
      assert [%{name: :"test alpha", outcome: :passed}] = results
    end

    test "returns empty list when filter matches nothing" do
      results = Interactive.run(Fixtures.TwoTests, deployment: :fake, test: "nonexistent")
      assert results == []
    end

    test "runs all tests when no filter given" do
      results = Interactive.run(Fixtures.TwoTests, deployment: :fake)
      assert length(results) == 2
    end
  end

  describe "process isolation" do
    test "each test runs in a separate process from the caller" do
      register_collector()
      caller_pid = self()
      Interactive.run(Fixtures.ProcessId, deployment: :fake)

      assert_received {:test_pid, test_pid}
      assert test_pid != caller_pid
    end
  end

  describe "summary output" do
    test "prints N passed, M failed" do
      # print_summary is only called for modules with setup_all
      defmodule SummaryModule do
        use ExUnit.Case, async: false

        setup_all do
          {:ok, %{}}
        end

        test "ok" do
          assert true
        end

        test "fail" do
          if Process.whereis(:interactive_test_collector),
            do: raise("boom")
        end
      end

      register_collector()

      output =
        capture_io(fn ->
          Interactive.run(SummaryModule, deployment: :fake)
        end)

      assert output =~ "1 passed, 1 failed"
    end

    test "prints summary for modules without setup_all" do
      output =
        capture_io(fn ->
          Interactive.run(Fixtures.Passing, deployment: :fake)
        end)

      assert output =~ "1 passed, 0 failed"
    end
  end

  describe "deployment registration" do
    test "registers deployment under :__standalone__ for modules without __toast_suite__" do
      Interactive.run(Fixtures.Passing, deployment: :my_deployment)
      assert DeploymentRegistry.get(:__standalone__) == :my_deployment
    end

    test "registers deployment under suite key for modules with __toast_suite__" do
      defmodule WithSuiteKey do
        use ExUnit.Case, async: false
        def __toast_suite__, do: :my_suite
        test "ok", do: assert(true)
      end

      Interactive.run(WithSuiteKey, deployment: :suite_deploy)
      assert DeploymentRegistry.get(:my_suite) == :suite_deploy
    end

    test "raises when :deployment option is missing" do
      assert_raise KeyError, fn ->
        Interactive.run(Fixtures.Passing, [])
      end
    end
  end
end
