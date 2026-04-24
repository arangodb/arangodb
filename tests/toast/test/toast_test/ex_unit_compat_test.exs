defmodule ToastTest.ExUnitCompatTest do
  use ExUnit.Case, async: true

  @moduletag :integration

  alias ToastTest.ExUnitCompat

  defp start_manager do
    {:ok, manager} = ExUnitCompat.start_event_manager()
    manager
  end

  describe "event manager" do
    test "start returns {:ok, manager}" do
      result = ExUnitCompat.start_event_manager()
      assert {:ok, manager} = result
      assert is_tuple(manager)
      assert :ok = ExUnitCompat.stop(manager)
    end

    test "suite lifecycle events" do
      manager = start_manager()

      assert :ok = ExUnitCompat.suite_started(manager, seed: 42, max_failures: :infinity)

      assert :ok =
               ExUnitCompat.suite_finished(manager, %{
                 run: 100_000,
                 async: 0,
                 load: 50_000
               })

      ExUnitCompat.stop(manager)
    end
  end

  describe "runner stats" do
    test "start and query stats" do
      manager = start_manager()
      result = ExUnitCompat.add_runner_stats(manager, [])
      assert {:ok, stats_pid} = result
      assert is_pid(stats_pid)

      stats = ExUnitCompat.stats(stats_pid)
      assert is_map(stats)

      ExUnitCompat.stop(manager)
    end

    test "failure counter increment and get" do
      manager = start_manager()
      {:ok, stats_pid} = ExUnitCompat.add_runner_stats(manager, [])

      assert ExUnitCompat.get_failure_counter(stats_pid) == 0

      ExUnitCompat.increment_failure_counter(stats_pid, 3)
      assert ExUnitCompat.get_failure_counter(stats_pid) == 3

      ExUnitCompat.increment_failure_counter(stats_pid, 2)
      assert ExUnitCompat.get_failure_counter(stats_pid) == 5

      ExUnitCompat.stop(manager)
    end
  end

  describe "formatter" do
    test "add_formatter accepts a formatter module with required opts" do
      manager = start_manager()
      ExUnitCompat.suite_started(manager, seed: 42, max_failures: :infinity)

      result =
        ExUnitCompat.add_formatter(manager, ExUnit.CLIFormatter,
          colors: [enabled: false],
          width: 80
        )

      assert {:ok, pid} = result
      assert is_pid(pid)
      ExUnitCompat.stop(manager)
    end
  end

  describe "test metadata" do
    test "get_test_metadata returns module test info" do
      metadata = ExUnitCompat.get_test_metadata(__MODULE__)
      assert %ExUnit.TestModule{} = metadata
      assert metadata.name == __MODULE__
      assert is_list(metadata.tests)
      assert metadata.tests != []
    end
  end

  describe "module and test events" do
    test "module_started and module_finished" do
      manager = start_manager()

      test_module = ExUnitCompat.get_test_metadata(__MODULE__)
      assert :ok = ExUnitCompat.module_started(manager, test_module)
      assert :ok = ExUnitCompat.module_finished(manager, test_module)

      ExUnitCompat.stop(manager)
    end

    test "test_started and test_finished" do
      manager = start_manager()

      test_module = ExUnitCompat.get_test_metadata(__MODULE__)
      test_case = hd(test_module.tests)
      assert :ok = ExUnitCompat.test_started(manager, test_case)
      assert :ok = ExUnitCompat.test_finished(manager, test_case)

      ExUnitCompat.stop(manager)
    end
  end

  describe "max_failures_reached" do
    test "sends max_failures event" do
      manager = start_manager()
      assert :ok = ExUnitCompat.max_failures_reached(manager)
      ExUnitCompat.stop(manager)
    end
  end
end
