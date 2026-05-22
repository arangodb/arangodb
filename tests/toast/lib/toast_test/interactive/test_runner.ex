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

defmodule ToastTest.Interactive.TestRunner do
  @moduledoc false

  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.TestLifecycle

  @on_exit_timeout 30_000

  @spec run_module_tests(module(), String.t() | nil) :: [map()]
  def run_module_tests(module, test_name) do
    test_module_meta = module.__ex_unit__()
    tests = filter_tests(test_module_meta.tests, test_name)
    run_with_lifecycle(test_module_meta, tests)
  end

  defp filter_tests(tests, nil), do: tests

  defp filter_tests(tests, pattern) do
    pattern = String.downcase(pattern)

    Enum.filter(tests, fn test ->
      test.name |> Atom.to_string() |> String.downcase() |> String.contains?(pattern)
    end)
  end

  defp run_with_lifecycle(%{setup_all?: false, name: module}, tests) do
    context = %{module: module, async: false}
    run_tests(module, tests, context)
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

    results
  end

  defp run_tests(module, tests, context) do
    Enum.map(tests, &run_single_test(module, &1, context))
  end

  defp run_single_test(module, test, setup_all_context) do
    parent_pid = self()

    {test_pid, test_ref} =
      spawn_monitor(fn ->
        Compat.register_on_exit(self())
        context = setup_all_context |> Map.merge(test.tags) |> Map.put(:test_pid, self())

        result =
          try do
            context = Compat.get_test_setup(module, context)
            apply(module, test.name, [context])

            case ToastTest.Expect.collect_failures() do
              [] -> :passed
              failures -> {:failed, {:error, %ExUnit.MultiError{errors: failures}, []}}
            end
          catch
            kind, error ->
              case ToastTest.Expect.collect_failures() do
                [] ->
                  {:failed, {kind, error, __STACKTRACE__}}

                prior ->
                  all = prior ++ [{kind, error, __STACKTRACE__}]
                  {:failed, {:error, %ExUnit.MultiError{errors: all}, []}}
              end
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
end
