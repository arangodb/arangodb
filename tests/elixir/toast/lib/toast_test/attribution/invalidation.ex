defmodule ToastTest.Attribution.Invalidation do
  @moduledoc """
  Neutralizes test failures that occurred after an unexpected server crash.

  Once the SUT has crashed, subsequent test results are no longer trustworthy
  — the crash monitor will abort the run, but there is a small race window
  during which one or more tests may continue executing and produce bogus
  failures. Instead of filtering these out silently, we rewrite their outcome
  to `:invalidated` and drop them from the failures list, so downstream
  surfaces (JUnit XML, outcomes.json, diagnostics ETF, CLI summary) can
  represent them consistently.

  A test is considered invalidated when its `started_at > earliest_crash_at`.
  The *trigger* test (started before the crash, still running when it
  happened) is left as `:failed` — its failure is the signal, not noise.
  """

  alias ToastTest.CrashEvent

  @spec invalidate(map(), [CrashEvent.t()]) :: map()
  def invalidate(test_data, []), do: test_data
  def invalidate(%{failures: []} = test_data, _crash_events), do: test_data

  def invalidate(test_data, crash_events) do
    case earliest_crash_us(crash_events) do
      nil ->
        test_data

      crash_us ->
        {modules, invalidated_keys} = rewrite_modules(test_data.modules, crash_us)
        failures = prune_failures(test_data.failures, invalidated_keys)
        %{test_data | modules: modules, failures: failures}
    end
  end

  # --- Private helpers ---

  defp earliest_crash_us(crash_events) do
    crash_events
    |> Enum.map(& &1.crash_info.timestamp)
    |> Enum.reject(&is_nil/1)
    |> Enum.min(fn -> nil end)
  end

  defp rewrite_modules(modules, crash_us) do
    Enum.reduce(modules, {%{}, MapSet.new()}, fn {mod, mod_result}, {acc, keys} ->
      {tests, keys} = rewrite_tests(mod, mod_result.tests, crash_us, keys)
      {Map.put(acc, mod, %{mod_result | tests: tests}), keys}
    end)
  end

  defp rewrite_tests(mod, tests, crash_us, keys) do
    Enum.map_reduce(tests, keys, fn test, keys ->
      if invalidate?(test, crash_us) do
        {%{test | outcome: :invalidated}, MapSet.put(keys, {mod, test.name})}
      else
        {test, keys}
      end
    end)
  end

  defp invalidate?(%{outcome: :failed, started_at: %DateTime{} = started_at}, crash_us) do
    DateTime.to_unix(started_at, :microsecond) > crash_us
  end

  defp invalidate?(_test, _crash_us), do: false

  defp prune_failures(failures, invalidated_keys) do
    Enum.reject(failures, fn %ExUnit.Test{module: mod, name: name} ->
      MapSet.member?(invalidated_keys, {mod, name})
    end)
  end
end
