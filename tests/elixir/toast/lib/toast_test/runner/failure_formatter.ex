defmodule ToastTest.Runner.FailureFormatter do
  @moduledoc false

  @runner_modules [
    ExUnit.Runner,
    ToastTest.Runner,
    ToastTest.Runner.TestExecution,
    ToastTest.Runner.TestProcess
  ]

  @spec prune_stacktrace(Exception.stacktrace()) :: Exception.stacktrace()
  def prune_stacktrace([{ExUnit.Assertions, _, _, _} | t]), do: prune_stacktrace(t)
  def prune_stacktrace([{mod, _, _, _} | _]) when mod in @runner_modules, do: []
  def prune_stacktrace([h | t]), do: [h | prune_stacktrace(t)]
  def prune_stacktrace([]), do: []

  @spec failed(atom() | {:EXIT, pid()}, term(), Exception.stacktrace()) :: {:failed, list()}
  def failed(:error, %ExUnit.MultiError{errors: errors}, _stack) do
    errors =
      Enum.map(errors, fn {kind, reason, stack} ->
        {kind, Exception.normalize(kind, reason, stack), prune_stacktrace(stack)}
      end)

    {:failed, errors}
  end

  def failed(kind, reason, stack) do
    {:failed, [{kind, Exception.normalize(kind, reason, stack), stack}]}
  end
end
