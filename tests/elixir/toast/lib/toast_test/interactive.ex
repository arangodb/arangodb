defmodule ToastTest.Interactive do
  @moduledoc false

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
        else: :interactive

    ensure_registry()
    ToastTest.DeploymentRegistry.put(suite_key, deployment)

    test_module = module.__ex_unit__()
    tests = filter_tests(test_module.tests, test_name)
    run_tests(module, tests)
  end

  defp ensure_registry do
    if :ets.whereis(:toast_deployment_registry) == :undefined do
      ToastTest.DeploymentRegistry.init()
    end
  end

  defp filter_tests(tests, nil), do: tests

  defp filter_tests(tests, name) do
    name_atom = String.to_atom("test #{name}")
    Enum.filter(tests, &(&1.name == name_atom))
  end

  defp run_tests(module, tests) do
    results =
      Enum.map(tests, fn test ->
        context = %{module: module, async: false, test_pid: self()}

        try do
          context = module.__ex_unit__(:setup, context)
          apply(module, test.name, [context])
          %{name: test.name, outcome: :passed, error: nil}
        catch
          kind, error ->
            %{name: test.name, outcome: :failed, error: {kind, error, __STACKTRACE__}}
        end
      end)

    passed = Enum.count(results, &(&1.outcome == :passed))
    failed = Enum.count(results, &(&1.outcome == :failed))
    IO.puts("#{passed} passed, #{failed} failed")
    results
  end
end
