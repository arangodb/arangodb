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

  @doc """
  Attribute a timestamp to the most specific scope.

  Returns `{scope, confidence}` where scope is one of:
  - `{:test, module, name}` with `:high` or `:low` confidence
  - `{:module, module}` with `nil` confidence
  - `:suite` with `nil` confidence

  ## Options
  - `:tolerance_us` — microseconds after test end for low-confidence match (default: 5_000_000)
  """
  @spec attribute(Toast.timestamp(), windows(), keyword()) ::
          {ToastTest.SuiteResult.scope(), :high | :low | nil}
  def attribute(timestamp, windows, opts \\ []) do
    tolerance_us = Keyword.get(opts, :tolerance_us, @default_tolerance_us)

    with :miss <- match_test(timestamp, windows.tests, tolerance_us),
         :miss <- match_module(timestamp, windows.modules) do
      {:suite, nil}
    end
  end

  # --- Test matching ---

  defp match_test(timestamp, tests, tolerance_us) do
    case find_high_match(timestamp, tests) do
      {:ok, {mod, name}} ->
        {{:test, mod, name}, :high}

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

        {{:test, mod, name}, :low}
    end
  end

  # --- Module matching ---

  defp match_module(timestamp, modules) do
    Enum.find_value(modules, :miss, fn {mod, window} ->
      if in_setup?(timestamp, window) or in_teardown?(timestamp, window),
        do: {{:module, mod}, nil}
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
      {{mod, test.name},
       %{started_at: to_us(test.started_at), finished_at: to_us(test.finished_at)}}
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
