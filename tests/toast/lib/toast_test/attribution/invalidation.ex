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

  A test is considered invalidated when its window started after the
  earliest crash (both timestamps from the EventStore timeline). The
  *trigger* test (started before the crash, still running when it happened)
  is left as `:failed` — its failure is the signal, not noise.
  """

  alias ToastTest.Attribution.TimeWindows
  alias ToastTest.CrashEvent

  @spec apply(map(), [CrashEvent.t()], TimeWindows.windows()) :: map()
  def apply(test_data, [], _windows), do: test_data
  def apply(%{failures: []} = test_data, _crash_events, _windows), do: test_data

  def apply(test_data, crash_events, windows) do
    case earliest_crash_us(crash_events) do
      nil ->
        test_data

      crash_us ->
        {modules, invalidated_keys} = rewrite_modules(test_data.modules, crash_us, windows)
        failures = prune_failures(test_data.failures, invalidated_keys)
        %{test_data | modules: modules, failures: failures}
    end
  end

  # Prefer the enriched, log-resolved crash time; fall back to the raw
  # detection time when the events have not been through enrichment.
  defp earliest_crash_us(crash_events) do
    crash_events
    |> Enum.map(&(&1.effective_at || &1.crash_info.timestamp))
    |> Enum.reject(&is_nil/1)
    |> Enum.min(fn -> nil end)
  end

  defp rewrite_modules(modules, crash_us, windows) do
    Enum.reduce(modules, {%{}, MapSet.new()}, fn {mod, mod_result}, {acc, keys} ->
      {tests, keys} = rewrite_tests(mod, mod_result.tests, crash_us, windows, keys)
      {Map.put(acc, mod, %{mod_result | tests: tests}), keys}
    end)
  end

  defp rewrite_tests(mod, tests, crash_us, windows, keys) do
    Enum.map_reduce(tests, keys, fn test, keys ->
      if invalidate?(mod, test, crash_us, windows) do
        {%{test | outcome: :invalidated}, MapSet.put(keys, {mod, test.name})}
      else
        {test, keys}
      end
    end)
  end

  defp invalidate?(mod, %{outcome: :failed, name: name}, crash_us, windows) do
    case windows.tests[{mod, name}] do
      %{started_at: started_us} -> started_us > crash_us
      nil -> false
    end
  end

  defp invalidate?(_mod, _test, _crash_us, _windows), do: false

  defp prune_failures(failures, invalidated_keys) do
    Enum.reject(failures, fn %ExUnit.Test{module: mod, name: name} ->
      MapSet.member?(invalidated_keys, {mod, name})
    end)
  end
end
