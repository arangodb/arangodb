defmodule Toast.ResultFormatter do
  @moduledoc "ExUnit formatter that collects test results for structured export."

  use GenServer

  @env_key :__test_results__

  # --- Client API ---

  @doc false
  def init(opts) do
    {:ok,
     %{
       suite_started_at: DateTime.utc_now(),
       tests: [],
       config: opts
     }}
  end

  # --- Event handling ---

  @doc false
  def handle_cast({:suite_started, _opts}, state) do
    {:noreply, %{state | suite_started_at: DateTime.utc_now()}}
  end

  def handle_cast({:test_finished, %ExUnit.Test{} = test}, state) do
    result = extract_test_result(test)
    {:noreply, %{state | tests: [result | state.tests]}}
  end

  def handle_cast({:suite_finished, times_us}, state) do
    results = %{
      suite_started_at: state.suite_started_at,
      suite_finished_at: DateTime.utc_now(),
      times_us: times_us,
      tests: Enum.reverse(state.tests)
    }

    Application.put_env(:toast, @env_key, results)
    {:noreply, state}
  end

  def handle_cast(_msg, state) do
    {:noreply, state}
  end

  # --- Result extraction ---

  defp extract_test_result(%ExUnit.Test{} = test) do
    {outcome, failure} = extract_outcome(test.state)

    %{
      module: test.module,
      name: to_string(test.name),
      outcome: outcome,
      duration_us: test.time,
      failure: failure,
      tags: %{
        file: test.tags[:file],
        line: test.tags[:line]
      }
    }
  end

  defp extract_outcome(nil), do: {:passed, nil}

  defp extract_outcome({:failed, failures}) do
    failure_info = Enum.map(failures, &format_failure/1)
    {:failed, failure_info}
  end

  defp extract_outcome({:skipped, msg}), do: {:skipped, %{message: msg}}
  defp extract_outcome({:excluded, msg}), do: {:excluded, %{message: msg}}
  defp extract_outcome({:invalid, _module}), do: {:invalid, nil}

  defp format_failure({:error, %{__struct__: mod} = error, stack}) do
    %{
      kind: inspect(mod),
      message: Exception.message(error),
      stacktrace: Exception.format_stacktrace(stack)
    }
  end

  defp format_failure({:error, reason, stack}) do
    %{
      kind: "ErlangError",
      message: inspect(reason),
      stacktrace: Exception.format_stacktrace(stack)
    }
  end

  defp format_failure({:exit, reason, stack}) do
    %{
      kind: "exit",
      message: inspect(reason),
      stacktrace: Exception.format_stacktrace(stack)
    }
  end

  defp format_failure({:throw, value, stack}) do
    %{
      kind: "throw",
      message: inspect(value),
      stacktrace: Exception.format_stacktrace(stack)
    }
  end
end
