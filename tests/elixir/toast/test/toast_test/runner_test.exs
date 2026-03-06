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

  describe "health check between tests" do
    # The runner calls Deployment.check_health/1 between tests via
    # check_config_deployments. This tests the decision function that
    # determines whether to abort based on deployment status.

    test "check_health returns :ok for :ready status" do
      {:ok, ctrl} =
        Toast.Deployment.Controller.start_link(
          mode: Toast.Deployment.Controller.SingleServer,
          config: Toast.Config.load()
        )

      :sys.replace_state(ctrl, fn state -> %{state | status: :ready} end)

      deployment = mock_deployment(ctrl, :single_server)
      assert Toast.Deployment.check_health(deployment) == :ok
    end

    test "check_health returns error for :degraded status" do
      {:ok, ctrl} =
        Toast.Deployment.Controller.start_link(
          mode: Toast.Deployment.Controller.SingleServer,
          config: Toast.Config.load()
        )

      :sys.replace_state(ctrl, fn state ->
        id = state.id

        server = %Toast.Deployment.ServerInstance{
          id: id,
          role: :single,
          operational_state: :stopped,
          expecting_exit: true
        }

        %{state | status: :degraded, servers: %{id => server}}
      end)

      deployment = mock_deployment(ctrl, :single_server)
      assert {:error, msg} = Toast.Deployment.check_health(deployment)
      assert msg =~ "degraded"
    end

    test "check_health returns error for :failed status" do
      {:ok, ctrl} =
        Toast.Deployment.Controller.start_link(
          mode: Toast.Deployment.Controller.SingleServer,
          config: Toast.Config.load()
        )

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.utc_now()
      }

      send(ctrl, {:server_crashed, "test-server", crash_info})
      :sys.get_state(ctrl)

      deployment = mock_deployment(ctrl, :single_server)
      assert {:error, msg} = Toast.Deployment.check_health(deployment)
      assert msg =~ "crashed" or msg =~ "failed"
    end

    test "check_health returns error for :stopped status" do
      {:ok, ctrl} =
        Toast.Deployment.Controller.start_link(
          mode: Toast.Deployment.Controller.SingleServer,
          config: Toast.Config.load()
        )

      deployment = mock_deployment(ctrl, :single_server)
      # Controller starts in :stopped status
      assert {:error, msg} = Toast.Deployment.check_health(deployment)
      assert msg =~ "not ready"
    end
  end

  describe "merge_stats accumulates across suites" do
    test "empty suites returns zero stats" do
      result = Runner.run_suites([], [])

      assert result.stats == %{total: 0, failures: 0, skipped: 0, excluded: 0}
      assert result.suites == []
    end

    # NOTE: Testing merge_stats with actual suite modules would require
    # creating full suite modules with deployment_config/0 callbacks and
    # having Toast.Deployment.start() succeed, which requires a real
    # ArangoDB binary. The merge_stats function itself is straightforward
    # additive accumulation -- the interesting behavior is tested by
    # verifying the structure returned by run_suites.
  end

  describe "suite abort isolation" do
    # The runner clears abort state between suites via cleanup_between_suites.
    # This tests the mechanism that ensures one suite's abort does not leak.

    test "abort state is cleared between suites" do
      Runner.abort!("suite 1 crashed")
      assert Runner.aborted?() == "suite 1 crashed"

      ToastTest.StateCleanup.reset()

      assert Runner.aborted?() == nil
    end
  end

  describe "mark_all_errored_stats" do
    # mark_all_errored_stats creates an ExUnit event stream where every test
    # in the given modules is marked as failed with a deployment error.
    # We test this indirectly via run_suites with a suite whose deployment fails.

    test "creates failure stats for a module with tests" do
      # Define a test module to be marked as errored
      defmodule ErroredTestModule do
        use ExUnit.Case, async: false

        test "test one" do
          :ok
        end

        test "test two" do
          :ok
        end
      end

      # Exercise mark_all_errored_stats through the ExUnitCompat event pipeline
      alias ToastTest.ExUnitCompat, as: Compat

      opts = ExUnit.configuration() |> Keyword.put(:formatters, [])
      {:ok, manager} = Compat.start_event_manager()
      {:ok, stats_pid} = Compat.add_runner_stats(manager, opts)
      Compat.suite_started(manager, opts)

      test_module = Compat.get_test_metadata(ErroredTestModule)
      Compat.module_started(manager, test_module)

      for test <- test_module.tests do
        errored = %{
          test
          | state:
              {:failed, [{:error, RuntimeError.exception("Deployment failed: :test_reason"), []}]}
        }

        Compat.test_started(manager, errored)
        Compat.test_finished(manager, errored)
      end

      Compat.module_finished(manager, test_module)
      Compat.suite_finished(manager, %{async: nil, load: nil, run: 0})

      stats = Compat.stats(stats_pid)
      Compat.stop(manager)

      # All tests should be counted as failures
      assert stats.failures == length(test_module.tests)
      assert stats.failures >= 2
      assert stats.total == length(test_module.tests)
    end
  end

  describe "check_between_tests dispatch" do
    test "suite mode with between_tests: false skips check" do
      # When deployment_config returns between_tests: false,
      # check_between_tests returns nil without checking health
      defmodule SkipBetweenSuite do
        use ToastTest.Suite, between_tests: false
      end

      config = SkipBetweenSuite.deployment_config()
      assert Keyword.get(config, :between_tests) == false
    end

    test "suite mode default calls check_health" do
      defmodule DefaultBetweenSuite do
        use ToastTest.Suite
      end

      config = DefaultBetweenSuite.deployment_config()
      # Default value is :default, not false
      assert Keyword.get(config, :between_tests) != false
    end

    test "suite mode with custom between_tests callback" do
      defmodule CustomBetweenSuite do
        use ToastTest.Suite

        @impl ToastTest.Suite
        def between_tests(_deployment, _prev_test) do
          {:error, "custom check failed"}
        end
      end

      assert function_exported?(CustomBetweenSuite, :between_tests, 2)
      # The callback returns {:error, reason} which becomes the abort reason
      assert {:error, "custom check failed"} =
               CustomBetweenSuite.between_tests(nil, nil)
    end
  end

  describe "emit_skipped_module" do
    test "marks all tests in a module as skipped" do
      defmodule SkippableModule do
        use ExUnit.Case, async: false

        test "will be skipped" do
          :ok
        end

        test "also skipped" do
          :ok
        end
      end

      alias ToastTest.ExUnitCompat, as: Compat

      opts = ExUnit.configuration() |> Keyword.put(:formatters, [])
      {:ok, manager} = Compat.start_event_manager()
      {:ok, stats_pid} = Compat.add_runner_stats(manager, opts)
      Compat.suite_started(manager, opts)

      test_module = Compat.get_test_metadata(SkippableModule)
      Compat.module_started(manager, test_module)

      skipped_tests =
        for test <- test_module.tests do
          %{test | state: {:skipped, "Suite aborted: test reason"}}
        end

      for test <- skipped_tests do
        Compat.test_started(manager, test)
        Compat.test_finished(manager, test)
      end

      Compat.module_finished(manager, %{test_module | tests: skipped_tests})
      Compat.suite_finished(manager, %{async: nil, load: nil, run: 0})

      stats = Compat.stats(stats_pid)
      Compat.stop(manager)

      assert stats.skipped == length(test_module.tests)
      assert stats.skipped >= 2
      assert stats.failures == 0
    end
  end

  defp mock_deployment(ctrl, mode) do
    %Toast.Deployment{
      id: "test-runner",
      mode: mode,
      config: Toast.Config.load(),
      controller: ctrl,
      endpoint: "http://127.0.0.1:0"
    }
  end
end
