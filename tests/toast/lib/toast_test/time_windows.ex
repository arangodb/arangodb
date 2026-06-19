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

defmodule ToastTest.TimeWindows do
  @moduledoc """
  Builds time windows from EventStore test lifecycle events and attributes
  timestamps to the most specific scope (test, module, or suite).

  Windows and the facts being attributed (crashes, sanitizer reports) both
  come from the EventStore timeline, so all comparisons are single-clock.

  Uses a two-tier confidence model for test attribution:
  - `:high` — timestamp falls within the test window
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
          modules: %{module() => module_window()},
          tests: %{
            {module(), atom()} => %{started_at: Toast.timestamp(), finished_at: Toast.timestamp()}
          }
        }

  @type module_window :: %{
          started_at: Toast.timestamp() | nil,
          setup_finished_at: Toast.timestamp() | nil,
          teardown_started_at: Toast.timestamp() | nil,
          finished_at: Toast.timestamp() | nil
        }

  @doc """
  Build time windows from EventStore events.

  Folds `:module_started`/`:module_finished`, `:test_started`/`:test_finished`,
  and `:between_tests_finished` events into per-module and per-test windows;
  all other events are ignored.

  A test window extends to its `:between_tests_finished` timestamp when
  present, so crashes detected during the between-tests barrier still
  attribute to the test. A test that started but never cleanly finished (e.g.
  the run aborted mid-test) is retained with a synthetic end = the last event
  timestamp in the stream, so a crash during it still attributes to that test
  (tests run sequentially, so a missing `:test_finished` means nothing started
  after it). Module setup ends at the first `:test_started`; teardown begins at
  the last `:test_finished`.
  """
  @spec build([map(), ...]) :: windows()
  def build([_ | _] = events) do
    %{modules: modules, tests: tests} =
      Enum.reduce(events, %{modules: %{}, tests: %{}}, &collect_event/2)

    %{modules: modules, tests: finalize_tests(tests, stream_end(events))}
  end

  # The last known instant in the run — the synthetic end for a still-running test.
  defp stream_end(events), do: events |> Enum.map(& &1.timestamp) |> Enum.max()

  @empty_module_window %{
    started_at: nil,
    finished_at: nil,
    setup_finished_at: nil,
    teardown_started_at: nil
  }

  defp collect_event(%{event: :module_started, module: m, timestamp: ts}, acc) do
    update_module(acc, m, fn
      %{started_at: nil} = w -> %{w | started_at: ts}
      w -> w
    end)
  end

  defp collect_event(%{event: :module_finished, module: m, timestamp: ts}, acc) do
    update_module(acc, m, &%{&1 | finished_at: ts})
  end

  defp collect_event(%{event: :test_started, module: m, name: n, timestamp: ts}, acc) do
    tests =
      Map.put(acc.tests, {m, n}, %{started_at: ts, finished_at: nil, barrier_finished_at: nil})

    update_module(%{acc | tests: tests}, m, fn
      %{setup_finished_at: nil} = w -> %{w | setup_finished_at: ts}
      w -> w
    end)
  end

  defp collect_event(%{event: :test_finished, module: m, name: n, timestamp: ts}, acc) do
    tests = Map.replace_lazy(acc.tests, {m, n}, &%{&1 | finished_at: ts})
    update_module(%{acc | tests: tests}, m, &%{&1 | teardown_started_at: ts})
  end

  defp collect_event(%{event: :between_tests_finished, module: m, name: n, timestamp: ts}, acc) do
    %{acc | tests: Map.replace_lazy(acc.tests, {m, n}, &%{&1 | barrier_finished_at: ts})}
  end

  defp collect_event(_event, acc), do: acc

  defp update_module(acc, m, fun) do
    window = Map.get(acc.modules, m, @empty_module_window)
    %{acc | modules: Map.put(acc.modules, m, fun.(window))}
  end

  defp finalize_tests(tests, stream_end) do
    for {key, %{started_at: s} = w} <- tests, s != nil, into: %{} do
      {key, %{started_at: s, finished_at: test_end(w, stream_end)}}
    end
  end

  # A finished test ends at its barrier-extended finish; a still-running test
  # (no `:test_finished`) ends at the stream's last instant.
  defp test_end(%{finished_at: nil}, stream_end), do: stream_end
  defp test_end(%{finished_at: f, barrier_finished_at: b}, _stream_end), do: b || f

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
      {:ok, {mod, name}} -> {{:test, mod, name}, :high, nil}
      :none -> find_low_match(timestamp, tests, tolerance_us)
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
       when not is_nil(started) and not is_nil(setup_end) do
    in_window?(timestamp, started, setup_end)
  end

  defp in_setup?(_timestamp, _window), do: false

  defp in_teardown?(timestamp, %{teardown_started_at: td_start, finished_at: finished})
       when not is_nil(td_start) and not is_nil(finished) do
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

  defp in_window?(timestamp, window_start, window_end) do
    timestamp >= window_start and timestamp <= window_end
  end

  defp after_window_within_tolerance?(timestamp, window_end, tolerance_us) do
    timestamp > window_end and timestamp - window_end <= tolerance_us
  end
end
