defmodule ToastTest.Runner.TestExecution do
  @moduledoc false

  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.Runner.{FailureFormatter, TestFilter, TestProcess, Timeout}
  alias ToastTest.{Abort, TestLifecycle, EventStore}

  require Logger

  @current_key ToastTest.Runner.current_key()

  def emit_module_with_state(manager, module, transform_test) do
    test_module = Compat.get_test_metadata(module)
    EventStore.notify(%{event: :module_started, module: module})
    Compat.module_started(manager, test_module)

    transformed_tests =
      for test <- test_module.tests do
        transformed = transform_test.(test)
        Compat.test_started(manager, transformed)
        Compat.test_finished(manager, transformed)
        transformed
      end

    Compat.module_finished(manager, %{test_module | tests: transformed_tests})
    EventStore.notify(%{event: :module_finished, module: module})
  end

  def max_failures_reached?(%{stats_pid: stats_pid, max_failures: max_failures}) do
    max_failures != :infinity and
      Compat.get_failure_counter(stats_pid) >= max_failures
  end

  def run_modules(config, test_modules) do
    Enum.each(test_modules, fn module ->
      Timeout.check_suite_deadline!(config)

      cond do
        reason = Abort.reason() ->
          emit_skipped_module(
            config,
            module,
            Abort.format_skip(reason)
          )

        max_failures_reached?(config) ->
          :ok

        true ->
          run_module(config, module)
      end
    end)
  end

  defp run_module(config, module) do
    test_module = Compat.get_test_metadata(module)
    EventStore.notify(%{event: :module_started, module: module})
    Compat.module_started(config.manager, test_module)

    {to_run_tests, excluded_and_skipped_tests} =
      TestFilter.filter(config.filters, test_module.tests)

    execute_module_tests(config, test_module, to_run_tests, excluded_and_skipped_tests)
  end

  defp execute_module_tests(config, test_module, to_run_tests, excluded_and_skipped_tests) do
    for excluded_or_skipped_test <- excluded_and_skipped_tests do
      Compat.test_started(config.manager, excluded_or_skipped_test)
      Compat.test_finished(config.manager, excluded_or_skipped_test)
    end

    {test_module, remaining_tests, finished_tests} =
      run_module_tests(config, test_module, to_run_tests)

    if reason = Abort.reason() do
      finish_aborted_module(config, test_module, remaining_tests, finished_tests, reason)
    else
      finish_pending_module(config, test_module, remaining_tests, finished_tests)
    end
  end

  defp run_module_tests(_config, test_module, []) do
    {test_module, [], []}
  end

  defp run_module_tests(config, test_module, tests) do
    Process.put(@current_key, test_module)
    %ExUnit.TestModule{name: module, tags: tags, parameters: params} = test_module

    context =
      tags
      |> Map.merge(params)
      |> Map.merge(%{module: module, async: Map.get(tags, :async, false)})

    config
    |> run_setup_all(test_module, context, fn context ->
      if max_failures_reached?(config) or Abort.reason(),
        do: {[], tests},
        else: run_tests(config, tests, test_module.parameters, context)
    end)
    |> case do
      {{:ok, {finished_tests, remaining_tests}}, test_module} ->
        {test_module, remaining_tests, finished_tests}

      {:error, test_module} ->
        log_setup_all_failure(test_module, length(tests))
        {test_module, Enum.map(tests, &%{&1 | state: {:invalid, test_module}}), []}
    end
  end

  defp log_setup_all_failure(%ExUnit.TestModule{name: module, state: {:failed, failures}}, count) do
    formatted =
      Enum.map_join(failures, "\n", fn {kind, error, stack} ->
        Exception.format(kind, error, stack)
      end)

    Logger.error(
      "setup_all failed for #{inspect(module)} — " <>
        "#{count} #{Toast.Utils.pluralize(count, "test")} invalidated\n#{formatted}"
    )
  end

  defp run_setup_all(
         _config,
         %ExUnit.TestModule{setup_all?: false} = test_module,
         context,
         callback
       ) do
    {{:ok, callback.(context)}, test_module}
  end

  defp run_setup_all(config, %ExUnit.TestModule{name: module} = test_module, context, callback) do
    {module_pid, module_ref} = TestLifecycle.spawn_setup_all(module, context)

    {ok_or_error, test_module} =
      receive do
        {^module_pid, :setup_all, {:ok, context}} ->
          finished_tests = callback.(context)
          :ok = TestLifecycle.exit_setup_all(module_pid, module_ref)
          {{:ok, finished_tests}, test_module}

        {^module_pid, :setup_all, {:error, {kind, error, stack}}} ->
          :ok = TestLifecycle.exit_setup_all(module_pid, module_ref)

          {:error,
           %{
             test_module
             | state:
                 FailureFormatter.failed(
                   kind,
                   error,
                   FailureFormatter.prune_stacktrace(stack)
                 )
           }}

        {:DOWN, ^module_ref, :process, ^module_pid, error} ->
          {:error,
           %{
             test_module
             | state: FailureFormatter.failed({:EXIT, module_pid}, error, [])
           }}
      end

    {timeout, _source} = Timeout.get_timeout(config, %{})
    {ok_or_error, TestProcess.exec_on_exit(test_module, module_pid, timeout)}
  end

  defp finish_aborted_module(config, test_module, remaining_tests, finished_tests, reason) do
    abort_msg = Abort.format_skip(reason)

    for test <- remaining_tests do
      skipped = %{test | state: {:skipped, abort_msg}}
      Compat.test_started(config.manager, skipped)
      Compat.test_finished(config.manager, skipped)
    end

    test_module = %{test_module | tests: Enum.reverse(finished_tests, remaining_tests)}
    Compat.module_finished(config.manager, test_module)
    EventStore.notify(%{event: :module_finished, module: test_module.name})
  end

  defp finish_pending_module(config, test_module, remaining_tests, finished_tests) do
    pending_tests =
      case process_max_failures(config, test_module) do
        :no -> remaining_tests
        {:reached, n} -> Enum.take(remaining_tests, n)
        :surpassed -> nil
      end

    if pending_tests do
      emit_pending_tests(config, test_module, pending_tests, finished_tests)
    end
  end

  defp emit_pending_tests(config, test_module, pending_tests, finished_tests) do
    for pending_test <- pending_tests do
      Compat.test_started(config.manager, pending_test)
      Compat.test_finished(config.manager, pending_test)
    end

    test_module = %{test_module | tests: Enum.reverse(finished_tests, pending_tests)}
    Compat.module_finished(config.manager, test_module)
    EventStore.notify(%{event: :module_finished, module: test_module.name})
  end

  defp emit_skipped_module(config, module, reason) do
    emit_module_with_state(config.manager, module, fn test ->
      %{test | state: {:skipped, reason}}
    end)
  end

  defp run_tests(config, tests, params, context) do
    run_tests_loop(config, tests, params, context, [])
  end

  defp run_tests_loop(config, [], _params, _context, acc) do
    # Gives the crash barrier one more chance to detect a crash whose :DOWN is
    # still pending after the module's last test.
    case acc do
      [last_test | _] ->
        case check_between_tests(config, last_test) do
          {:error, reason} -> Abort.abort!(reason)
          :ok -> :ok
        end

      [] ->
        :ok
    end

    {acc, []}
  end

  defp run_tests_loop(config, [test | rest] = remaining, params, context, acc) do
    Timeout.check_suite_deadline!(config)

    prev_test = List.first(acc)

    if Abort.reason() do
      {acc, remaining}
    else
      case check_between_tests(config, prev_test) do
        {:error, reason} ->
          Abort.abort!(reason)
          {acc, remaining}

        :ok ->
          run_next_test(config, test, rest, params, context, acc)
      end
    end
  end

  defp run_next_test(config, test, rest, params, context, acc) do
    test = %{test | parameters: params}
    Process.put(@current_key, test)

    case run_test(config, test, context) do
      {:ok, test} -> run_tests_loop(config, rest, params, context, [test | acc])
      :max_failures_reached -> {acc, rest}
    end
  end

  defp check_between_tests(%{between_tests: between_tests}, prev_test) do
    between_tests.(prev_test)
  end

  defp run_test(config, test, context) do
    EventStore.notify(%{
      event: :test_started,
      module: test.module,
      name: test.name
    })

    Compat.test_started(config.manager, test)
    test = TestProcess.spawn_test(config, test, context)

    EventStore.notify(%{
      event: :test_finished,
      module: test.module,
      name: test.name,
      outcome: ToastTest.Formatting.test_outcome(test),
      duration_us: test.time
    })

    case process_max_failures(config, test) do
      :no ->
        Compat.test_finished(config.manager, test)
        {:ok, test}

      {:reached, 1} ->
        Compat.test_finished(config.manager, test)
        :max_failures_reached

      :surpassed ->
        :max_failures_reached
    end
  end

  # Returns :no (under limit), {:reached, n} (just crossed — emit n more), or :surpassed (already past).
  defp process_max_failures(%{max_failures: :infinity}, _), do: :no

  defp process_max_failures(config, %ExUnit.TestModule{state: {:failed, _}, tests: tests}) do
    process_max_failures(config.stats_pid, config.max_failures, length(tests))
  end

  defp process_max_failures(config, %ExUnit.Test{state: {:failed, _}}) do
    process_max_failures(config.stats_pid, config.max_failures, 1)
  end

  defp process_max_failures(config, _test_module_or_test) do
    if max_failures_reached?(config), do: :surpassed, else: :no
  end

  defp process_max_failures(stats_pid, max_failures, bump) do
    previous = Compat.increment_failure_counter(stats_pid, bump)

    cond do
      previous >= max_failures -> :surpassed
      previous + bump < max_failures -> :no
      true -> {:reached, max_failures - previous}
    end
  end
end
