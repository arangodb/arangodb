defmodule ToastTest.Interactive do
  @moduledoc "Quick interactive debugging helper that runs tests with proper ExUnit lifecycle support (setup_all, setup, on_exit)."

  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.TestLifecycle

  @on_exit_timeout 30_000

  @spec run(module() | String.t(), keyword()) :: [map()]
  def run(module_or_path, opts \\ [])

  def run(path, opts) when is_binary(path) do
    [{module, _}] = Code.compile_file(path)
    run(module, opts)
  end

  def run(module, opts) when is_atom(module) do
    deployment = Keyword.fetch!(opts, :deployment)
    test_name = Keyword.get(opts, :test)

    suite_key =
      if function_exported?(module, :__toast_suite__, 0),
        do: module.__toast_suite__(),
        else: :__standalone__

    ToastTest.DeploymentRegistry.ensure_init()
    ToastTest.DeploymentRegistry.put(suite_key, deployment)

    test_module = module.__ex_unit__()
    tests = filter_tests(test_module.tests, test_name)
    run_with_lifecycle(test_module, tests)
  end

  defp filter_tests(tests, nil), do: tests

  defp filter_tests(tests, name) do
    name_atom = String.to_atom("test #{name}")
    Enum.filter(tests, &(&1.name == name_atom))
  end

  defp run_with_lifecycle(%{setup_all?: false, name: module}, tests) do
    context = %{module: module, async: false}
    results = run_tests(module, tests, context)
    print_summary(results)
    results
  end

  defp run_with_lifecycle(%{name: module}, tests) do
    context = %{module: module, async: false}

    {module_pid, module_ref} = TestLifecycle.spawn_setup_all(module, context)

    {results, setup_all_error} =
      receive do
        {^module_pid, :setup_all, {:ok, setup_all_context}} ->
          results = run_tests(module, tests, setup_all_context)
          TestLifecycle.exit_setup_all(module_pid, module_ref)
          {results, nil}

        {^module_pid, :setup_all, {:error, error}} ->
          TestLifecycle.exit_setup_all(module_pid, module_ref)
          {mark_all_failed(tests, module, error), error}

        {:DOWN, ^module_ref, :process, ^module_pid, error} ->
          failure = {:EXIT, error, []}
          {mark_all_failed(tests, module, failure), failure}
      end

    on_exit_error = run_on_exit(module_pid)

    if on_exit_error && is_nil(setup_all_error) do
      IO.puts("Warning: setup_all on_exit handler failed: #{inspect(on_exit_error)}")
    end

    print_summary(results)
    results
  end

  defp run_tests(module, tests, context) do
    Enum.map(tests, &run_single_test(module, &1, context))
  end

  defp run_single_test(module, test, setup_all_context) do
    parent_pid = self()

    {test_pid, test_ref} =
      spawn_monitor(fn ->
        ExUnit.OnExitHandler.register(self())
        context = setup_all_context |> Map.merge(test.tags) |> Map.put(:test_pid, self())

        result =
          try do
            context = Compat.get_test_setup(module, context)
            apply(module, test.name, [context])
            :passed
          catch
            kind, error ->
              {:failed, {kind, error, __STACKTRACE__}}
          end

        send(parent_pid, {self(), :test_finished, result})
        exit(:shutdown)
      end)

    result =
      receive do
        {^test_pid, :test_finished, :passed} ->
          Process.demonitor(test_ref, [:flush])
          %{module: module, name: test.name, outcome: :passed, failure: nil}

        {^test_pid, :test_finished, {:failed, error}} ->
          Process.demonitor(test_ref, [:flush])
          %{module: module, name: test.name, outcome: :failed, failure: error}

        {:DOWN, ^test_ref, :process, ^test_pid, error} ->
          %{
            module: module,
            name: test.name,
            outcome: :failed,
            failure: {:EXIT, error, []}
          }
      end

    case run_on_exit(test_pid) do
      nil ->
        result

      on_exit_error when result.outcome == :passed ->
        %{result | outcome: :failed, failure: on_exit_error}

      _on_exit_error ->
        result
    end
  end

  defp run_on_exit(pid) do
    case TestLifecycle.run_on_exit(pid, @on_exit_timeout) do
      :ok -> nil
      error -> error
    end
  end

  defp mark_all_failed(tests, module, failure) do
    Enum.map(tests, &%{module: module, name: &1.name, outcome: :failed, failure: failure})
  end

  defp print_summary(results) do
    passed = Enum.count(results, &(&1.outcome == :passed))
    failed = Enum.count(results, &(&1.outcome == :failed))
    IO.puts("#{passed} passed, #{failed} failed")
  end
end
