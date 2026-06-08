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

defmodule ToastTest.Attribution.TimeWindows do
  @moduledoc """
  Builds time windows from test execution data and attributes timestamps
  to the most specific scope (test, module, or suite).

  Uses a two-tier confidence model for test attribution:
  - `:high` — timestamp falls within `[test.started_at, test.finished_at]`
  - `:low` — timestamp is within a tolerance window after test end

  All timestamps are `t:Toast.timestamp/0` (Unix microseconds).
  """

  @default_tolerance_us 5_000_000

  @usec_per_ms 1_000

  @doc "Apply millisecond padding to a microsecond window."
  @spec pad(integer(), integer(), {integer(), integer()}) :: {integer(), integer()}
  def pad(start_us, end_us, {before_ms, after_ms}) do
    {start_us + before_ms * @usec_per_ms, end_us + after_ms * @usec_per_ms}
  end

  @doc "Look up `type` in `padding_map` and apply the padding."
  @spec pad(integer(), integer(), atom(), %{atom() => {integer(), integer()}}) ::
          {integer(), integer()}
  def pad(start_us, end_us, type, padding_map) do
    pad(start_us, end_us, Map.fetch!(padding_map, type))
  end

  @type windows :: %{
          suite: %{started_at: Toast.timestamp(), finished_at: Toast.timestamp()},
          modules: %{module() => module_window()},
          tests: %{
            {module(), atom()} => %{started_at: Toast.timestamp(), finished_at: Toast.timestamp()}
          }
        }

  @type module_window :: %{
          started_at: Toast.timestamp(),
          setup_finished_at: Toast.timestamp() | nil,
          teardown_started_at: Toast.timestamp() | nil,
          finished_at: Toast.timestamp()
        }

  @doc """
  Build time windows from ResultCollector test data.

  Converts DateTime values from the collector into `t:Toast.timestamp/0`.
  """
  @spec build(ToastTest.ResultCollector.test_data()) :: windows()
  def build(test_data) do
    %{
      suite: %{
        started_at: to_us(test_data.started_at),
        finished_at: to_us(test_data.finished_at)
      },
      modules: build_module_windows(test_data.modules),
      tests: build_test_windows(test_data.modules)
    }
  end

  @type phase :: :setup | :teardown | nil

  @doc """
  Attribute a timestamp to the most specific scope.

  Returns `{scope, confidence, phase}` where scope is one of:
  - `{:test, module, name}` with `:high` or `:low` confidence, phase `nil`
  - `{:module, module}` with `nil` confidence, phase `:setup` or `:teardown`
  - `:suite` with `nil` confidence, phase `nil`

  ## Options
  - `:tolerance_us` — microseconds after test end for low-confidence match (default: 5_000_000)
  """
  @spec attribute(Toast.timestamp(), windows(), keyword()) ::
          {ToastTest.SuiteResult.scope(), :high | :low | nil, phase()}
  def attribute(timestamp, windows, opts \\ []) do
    tolerance_us = Keyword.get(opts, :tolerance_us, @default_tolerance_us)

    with :miss <- match_test(timestamp, windows.tests, tolerance_us),
         :miss <- match_module(timestamp, windows.modules) do
      {:suite, nil, suite_phase(timestamp, windows.modules)}
    end
  end

  # --- Test matching ---

  defp match_test(timestamp, tests, tolerance_us) do
    case find_high_match(timestamp, tests) do
      {:ok, {mod, name}} ->
        {{:test, mod, name}, :high, nil}

      :none ->
        find_low_match(timestamp, tests, tolerance_us)
    end
  end

  defp find_high_match(timestamp, tests) do
    Enum.find_value(tests, :none, fn {{mod, name}, window} ->
      if in_window?(timestamp, window.started_at, window.finished_at),
        do: {:ok, {mod, name}}
    end)
  end

  defp find_low_match(timestamp, tests, tolerance_us) do
    tests
    |> Enum.filter(fn {_key, window} ->
      after_window_within_tolerance?(timestamp, window.finished_at, tolerance_us)
    end)
    |> case do
      [] ->
        :miss

      candidates ->
        {{mod, name}, _window} =
          Enum.min_by(candidates, fn {_key, window} ->
            timestamp - window.finished_at
          end)

        {{:test, mod, name}, :low, nil}
    end
  end

  # --- Module matching ---

  defp match_module(timestamp, modules) do
    Enum.find_value(modules, :miss, fn {mod, window} ->
      cond do
        in_setup?(timestamp, window) -> {{:module, mod}, nil, :setup}
        in_teardown?(timestamp, window) -> {{:module, mod}, nil, :teardown}
        true -> nil
      end
    end)
  end

  defp in_setup?(timestamp, %{started_at: started, setup_finished_at: setup_end})
       when not is_nil(setup_end) do
    in_window?(timestamp, started, setup_end)
  end

  defp in_setup?(_timestamp, _window), do: false

  defp in_teardown?(timestamp, %{teardown_started_at: td_start, finished_at: finished})
       when not is_nil(td_start) do
    in_window?(timestamp, td_start, finished)
  end

  defp in_teardown?(_timestamp, _window), do: false

  # Determines where in the suite lifecycle a suite-scoped timestamp falls.
  defp suite_phase(_timestamp, modules) when map_size(modules) == 0, do: nil

  defp suite_phase(timestamp, modules) do
    {earliest_start, latest_finish} =
      Enum.reduce(modules, {nil, nil}, fn {_mod, w}, {min_s, max_f} ->
        {min_ts(min_s, w.started_at), max_ts(max_f, w.finished_at)}
      end)

    cond do
      earliest_start && timestamp < earliest_start -> :startup
      latest_finish && timestamp > latest_finish -> :shutdown
      true -> nil
    end
  end

  defp min_ts(nil, ts), do: ts
  defp min_ts(ts, nil), do: ts
  defp min_ts(a, b), do: min(a, b)

  defp max_ts(nil, ts), do: ts
  defp max_ts(ts, nil), do: ts
  defp max_ts(a, b), do: max(a, b)

  # --- Window helpers ---

  defp build_module_windows(modules) do
    Map.new(modules, fn {mod, data} ->
      {mod,
       %{
         started_at: to_us(data.started_at),
         setup_finished_at: to_us(data.setup_finished_at),
         teardown_started_at: to_us(data.teardown_started_at),
         finished_at: to_us(data.finished_at)
       }}
    end)
  end

  defp build_test_windows(modules) do
    for {mod, data} <- modules,
        test <- data.tests,
        test.started_at != nil and test.finished_at != nil,
        into: %{} do
      effective_finish = Map.get(test, :between_tests_finished_at) || test.finished_at

      {{mod, test.name},
       %{started_at: to_us(test.started_at), finished_at: to_us(effective_finish)}}
    end
  end

  defp in_window?(timestamp, window_start, window_end) do
    timestamp >= window_start and timestamp <= window_end
  end

  defp after_window_within_tolerance?(timestamp, window_end, tolerance_us) do
    timestamp > window_end and timestamp - window_end <= tolerance_us
  end

  defp to_us(%DateTime{} = dt), do: DateTime.to_unix(dt, :microsecond)
  defp to_us(nil), do: nil
end
