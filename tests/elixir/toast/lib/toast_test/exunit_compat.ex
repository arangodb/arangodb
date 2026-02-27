defmodule ToastTest.ExUnitCompat do
  @supported_elixir "~> 1.19"

  unless Version.match?(System.version(), @supported_elixir) do
    IO.warn("ToastTest.ExUnitCompat: untested Elixir version #{System.version()}")
  end

  @spec start_event_manager() :: {:ok, pid()}
  def start_event_manager do
    ExUnit.EventManager.start_link()
  end

  @spec add_runner_stats(pid(), keyword()) :: {:ok, pid()}
  def add_runner_stats(manager, opts) do
    ExUnit.EventManager.add_handler(manager, ExUnit.RunnerStats, opts)
  end

  @spec add_formatter(pid(), module(), keyword()) :: {:ok, pid()}
  def add_formatter(manager, formatter, opts) do
    ExUnit.EventManager.add_handler(manager, formatter, opts)
  end

  @spec suite_started(pid(), keyword()) :: :ok
  def suite_started(manager, opts) do
    ExUnit.EventManager.suite_started(manager, opts)
  end

  @spec suite_finished(pid(), map()) :: :ok
  def suite_finished(manager, times_us) do
    ExUnit.EventManager.suite_finished(manager, times_us)
  end

  @spec module_started(pid(), ExUnit.TestModule.t()) :: :ok
  def module_started(manager, test_module) do
    ExUnit.EventManager.module_started(manager, test_module)
  end

  @spec module_finished(pid(), ExUnit.TestModule.t()) :: :ok
  def module_finished(manager, test_module) do
    ExUnit.EventManager.module_finished(manager, test_module)
  end

  @spec test_started(pid(), ExUnit.Test.t()) :: :ok
  def test_started(manager, test) do
    ExUnit.EventManager.test_started(manager, test)
  end

  @spec test_finished(pid(), ExUnit.Test.t()) :: :ok
  def test_finished(manager, test) do
    ExUnit.EventManager.test_finished(manager, test)
  end

  @spec max_failures_reached(pid()) :: :ok
  def max_failures_reached(manager) do
    ExUnit.EventManager.max_failures_reached(manager)
  end

  @spec sigquit(pid(), term()) :: :ok
  def sigquit(manager, current) do
    ExUnit.EventManager.sigquit(manager, current)
  end

  @spec stop(pid()) :: :ok
  def stop(manager) do
    ExUnit.EventManager.stop(manager)
  end

  @spec stats(pid()) :: map()
  def stats(stats_pid) do
    ExUnit.RunnerStats.stats(stats_pid)
  end

  @spec increment_failure_counter(pid(), non_neg_integer()) :: non_neg_integer()
  def increment_failure_counter(stats_pid, bump) do
    ExUnit.RunnerStats.increment_failure_counter(stats_pid, bump)
  end

  @spec get_failure_counter(pid()) :: non_neg_integer()
  def get_failure_counter(stats_pid) do
    ExUnit.RunnerStats.get_failure_counter(stats_pid)
  end

  @spec get_test_metadata(module()) :: ExUnit.TestModule.t()
  def get_test_metadata(module) do
    module.__ex_unit__()
  end

  @spec get_test_setup(module(), map()) :: map()
  def get_test_setup(module, context) do
    module.__ex_unit__(:setup, context)
  end

  @spec get_setup_all(module(), map()) :: map()
  def get_setup_all(module, context) do
    module.__ex_unit__(:setup_all, context)
  end
end
