defmodule ToastTest.ExUnitCompat do
  @supported_elixir "~> 1.19"

  unless Version.match?(System.version(), @supported_elixir) do
    IO.warn("ToastTest.ExUnitCompat: untested Elixir version #{System.version()}")
  end

  def start_event_manager do
    ExUnit.EventManager.start_link()
  end

  def add_runner_stats(manager, opts) do
    ExUnit.EventManager.add_handler(manager, ExUnit.RunnerStats, opts)
  end

  def add_formatter(manager, formatter, opts) do
    ExUnit.EventManager.add_handler(manager, formatter, opts)
  end

  def suite_started(manager, opts) do
    ExUnit.EventManager.suite_started(manager, opts)
  end

  def suite_finished(manager, times_us) do
    ExUnit.EventManager.suite_finished(manager, times_us)
  end

  def module_started(manager, test_module) do
    ExUnit.EventManager.module_started(manager, test_module)
  end

  def module_finished(manager, test_module) do
    ExUnit.EventManager.module_finished(manager, test_module)
  end

  def test_started(manager, test) do
    ExUnit.EventManager.test_started(manager, test)
  end

  def test_finished(manager, test) do
    ExUnit.EventManager.test_finished(manager, test)
  end

  def max_failures_reached(manager) do
    ExUnit.EventManager.max_failures_reached(manager)
  end

  def sigquit(manager, current) do
    ExUnit.EventManager.sigquit(manager, current)
  end

  def stop(manager) do
    ExUnit.EventManager.stop(manager)
  end

  def stats(stats_pid) do
    ExUnit.RunnerStats.stats(stats_pid)
  end

  def increment_failure_counter(stats_pid, bump) do
    ExUnit.RunnerStats.increment_failure_counter(stats_pid, bump)
  end

  def get_failure_counter(stats_pid) do
    ExUnit.RunnerStats.get_failure_counter(stats_pid)
  end

  def get_test_metadata(module) do
    module.__ex_unit__()
  end

  def get_test_setup(module, context) do
    module.__ex_unit__(:setup, context)
  end

  def get_setup_all(module, context) do
    module.__ex_unit__(:setup_all, context)
  end
end
