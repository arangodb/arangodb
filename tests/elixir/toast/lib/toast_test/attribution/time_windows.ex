defmodule ToastTest.Attribution.TimeWindows do
  @moduledoc """
  Builds time windows from test execution data and attributes timestamps
  to the most specific scope (test, module, or suite).

  Uses a two-tier confidence model for test attribution:
  - `:high` — timestamp falls within `[test.started_at, test.finished_at]`
  - `:low` — timestamp is within a tolerance window after test end
  """

  @default_tolerance_s 5

  @type windows :: %{
          suite: %{started_at: DateTime.t(), finished_at: DateTime.t()},
          modules: %{module() => module_window()},
          tests: %{{module(), atom()} => %{started_at: DateTime.t(), finished_at: DateTime.t()}}
        }

  @type module_window :: %{
          started_at: DateTime.t(),
          setup_finished_at: DateTime.t() | nil,
          teardown_started_at: DateTime.t() | nil,
          finished_at: DateTime.t()
        }

  @doc """
  Build time windows from ResultCollector test data.
  """
  @spec build(ToastTest.ResultCollector.test_data()) :: windows()
  def build(test_data) do
    %{
      suite: %{started_at: test_data.started_at, finished_at: test_data.finished_at},
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
  - `:tolerance_s` — seconds after test end for low-confidence match (default: 5)
  """
  @spec attribute(DateTime.t(), windows(), keyword()) ::
          {ToastTest.SuiteResult.scope(), :high | :low | nil}
  def attribute(timestamp, windows, opts \\ []) do
    tolerance_s = Keyword.get(opts, :tolerance_s, @default_tolerance_s)

    with :miss <- match_test(timestamp, windows.tests, tolerance_s),
         :miss <- match_module(timestamp, windows.modules) do
      {:suite, nil}
    end
  end

  # --- Test matching ---

  defp match_test(timestamp, tests, tolerance_s) do
    # First pass: look for :high confidence match
    case find_high_match(timestamp, tests) do
      {:ok, {mod, name}} ->
        {{:test, mod, name}, :high}

      :none ->
        find_low_match(timestamp, tests, tolerance_s)
    end
  end

  defp find_high_match(timestamp, tests) do
    Enum.find_value(tests, :none, fn {{mod, name}, window} ->
      if in_window?(timestamp, window.started_at, window.finished_at),
        do: {:ok, {mod, name}}
    end)
  end

  defp find_low_match(timestamp, tests, tolerance_s) do
    tests
    |> Enum.filter(fn {_key, window} ->
      after_window_within_tolerance?(timestamp, window.finished_at, tolerance_s)
    end)
    |> case do
      [] ->
        :miss

      candidates ->
        # Pick the test whose end is closest (smallest gap)
        {{mod, name}, _window} =
          Enum.min_by(candidates, fn {_key, window} ->
            DateTime.diff(timestamp, window.finished_at, :millisecond)
          end)

        {{:test, mod, name}, :low}
    end
  end

  # --- Module matching ---

  defp match_module(timestamp, modules) do
    Enum.find_value(modules, :miss, fn {mod, window} ->
      cond do
        in_setup?(timestamp, window) -> {{:module, mod}, nil}
        in_teardown?(timestamp, window) -> {{:module, mod}, nil}
        true -> nil
      end
    end)
  end

  defp in_setup?(timestamp, %{started_at: started, setup_finished_at: setup_end})
       when not is_nil(setup_end) do
    in_window?(timestamp, started, setup_end) and
      DateTime.compare(timestamp, setup_end) == :lt
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
         started_at: data.started_at,
         setup_finished_at: data.setup_finished_at,
         teardown_started_at: data.teardown_started_at,
         finished_at: data.finished_at
       }}
    end)
  end

  defp build_test_windows(modules) do
    Enum.flat_map(modules, fn {mod, data} ->
      Enum.map(data.tests, fn test ->
        {{mod, test.name}, %{started_at: test.started_at, finished_at: test.finished_at}}
      end)
    end)
    |> Map.new()
  end

  defp in_window?(timestamp, window_start, window_end) do
    DateTime.compare(timestamp, window_start) in [:eq, :gt] and
      DateTime.compare(timestamp, window_end) in [:eq, :lt]
  end

  defp after_window_within_tolerance?(timestamp, window_end, tolerance_s) do
    DateTime.compare(timestamp, window_end) == :gt and
      DateTime.diff(timestamp, window_end, :millisecond) <= tolerance_s * 1000
  end
end
