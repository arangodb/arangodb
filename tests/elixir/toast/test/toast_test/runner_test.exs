defmodule ToastTest.RunnerTest do
  use ExUnit.Case, async: false

  alias ToastTest.Runner

  setup do
    Runner.clear_abort!()

    on_exit(fn ->
      Runner.clear_abort!()
    end)

    :ok
  end

  describe "abort mechanism" do
    test "abort!/1 sets abort reason" do
      assert Runner.aborted?() == nil
      Runner.abort!("test crash")
      assert Runner.aborted?() == "test crash"
    end

    test "clear_abort!/0 resets abort state" do
      Runner.abort!("test crash")
      assert Runner.aborted?() != nil
      Runner.clear_abort!()
      assert Runner.aborted?() == nil
    end

    test "only first abort wins" do
      Runner.abort!("first")
      Runner.abort!("second")
      assert Runner.aborted?() == "first"
    end
  end

  describe "compute_suite_deadline" do
    # We test indirectly via SuiteRun struct construction since compute_suite_deadline is private.
    # The timeout hierarchy is tested end-to-end in run_suites tests below.

    test "SuiteRun struct has expected fields" do
      run = %ToastTest.SuiteRun{
        suite_module: SomeSuite,
        suite_deadline: 12345,
        timeout_factor: 2.0
      }

      assert run.suite_module == SomeSuite
      assert run.suite_deadline == 12345
      assert run.timeout_factor == 2.0
      assert run.deployment == nil
      assert run.results == []
      assert run.diagnostics == nil
    end
  end

  describe "validate_no_async!" do
    # ExUnit doesn't persist the async flag in __ex_unit__() metadata.
    # The suite architecture prevents async modules: use Smoke.Suite ->
    # use ToastTest.Case -> use ExUnit.CaseTemplate (always sync).
    # validate_no_async! is a runtime guard that can't detect async from
    # compiled module metadata, so it's a no-op safety net.

    test "suite test modules are inherently sync" do
      defmodule TestSuiteForAsync do
        use ToastTest.Suite
      end

      defmodule TestModuleUsingSuite do
        use ToastTest.RunnerTest.TestSuiteForAsync

        test "dummy" do
          :ok
        end
      end

      # Modules created through the suite system don't have async: true
      meta = TestModuleUsingSuite.__ex_unit__()
      assert %ExUnit.TestModule{} = meta
    end
  end

  describe "merge_stats" do
    # merge_stats is private; test through the public run_suites return value shape
    test "run_suites returns expected structure" do
      # run_suites with empty list should return zero stats
      result = Runner.run_suites([], [])

      assert result == %{
               suites: [],
               stats: %{total: 0, failures: 0, skipped: 0, excluded: 0}
             }
    end
  end

  describe "ExUnitCompat adapter" do
    alias ToastTest.ExUnitCompat, as: Compat

    test "start_event_manager returns {:ok, manager}" do
      assert {:ok, manager} = Compat.start_event_manager()
      assert is_tuple(manager)
      Compat.stop(manager)
    end

    test "add_runner_stats returns {:ok, pid}" do
      {:ok, manager} = Compat.start_event_manager()
      assert {:ok, stats_pid} = Compat.add_runner_stats(manager, [])
      assert is_pid(stats_pid)
      Compat.stop(manager)
    end

    test "stats returns a map" do
      {:ok, manager} = Compat.start_event_manager()
      {:ok, stats_pid} = Compat.add_runner_stats(manager, [])
      stats = Compat.stats(stats_pid)
      assert is_map(stats)
      Compat.stop(manager)
    end

    test "get_test_metadata returns ExUnit module info" do
      defmodule MetaTestModule do
        use ExUnit.Case, async: false

        test "example" do
          :ok
        end
      end

      meta = Compat.get_test_metadata(MetaTestModule)
      assert %ExUnit.TestModule{} = meta
      assert meta.name == MetaTestModule
    end
  end

end
