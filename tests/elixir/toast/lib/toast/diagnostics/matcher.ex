defmodule Toast.Diagnostics.Matcher do
  @moduledoc """
  Shared timestamp-based matching of diagnostic items to test cases.

  Both sanitizer errors and crash reports carry timestamps. This module
  provides the core algorithm: given a list of timestamped items and a
  list of test results, match each item to the test whose execution
  window best contains the item's timestamp.

  Uses a two-tier confidence model:
  - **high**: timestamp falls within `[test_start, test_end]`
  - **low**: timestamp is within a tolerance window after test end
  """

  @default_tolerance_seconds 5

  @type confidence :: :high | :low | :none

  @doc """
  Calculate match confidence between a timestamp and a test window.

  Returns `:high` if the timestamp falls within `[test_start, test_end]`,
  `:low` if within `tolerance_seconds` after `test_end`, or `:none`.
  """
  @spec calculate_confidence(DateTime.t(), DateTime.t(), DateTime.t(), number()) :: confidence()
  def calculate_confidence(
        timestamp,
        test_start,
        test_end,
        tolerance_seconds \\ @default_tolerance_seconds
      ) do
    cond do
      DateTime.compare(timestamp, test_start) in [:gt, :eq] and
          DateTime.compare(timestamp, test_end) in [:lt, :eq] ->
        :high

      DateTime.compare(timestamp, test_end) == :gt and
          DateTime.diff(timestamp, test_end, :millisecond) <= tolerance_seconds * 1000 ->
        :low

      true ->
        :none
    end
  end

  @doc """
  Match timestamped items to test cases.

  `items` is a list of maps, each with a `:timestamp` field. `item_key`
  determines the key used in match entries (e.g., `:error` or `:crash`).

  Returns `%{matched: [%{module, test, confidence, <item_key>: item}], unmatched: [item]}`.

  ## Options

    * `:tolerance_seconds` — seconds after test end for low-confidence match (default: 5)
  """
  @spec match([map()], map() | nil, atom(), keyword()) :: map()
  def match(items, test_results, item_key, opts \\ [])

  def match([], _test_results, _item_key, _opts), do: empty_result()
  def match(_items, nil, _item_key, _opts), do: empty_result()

  def match(items, test_results, item_key, opts) do
    tolerance = Keyword.get(opts, :tolerance_seconds, @default_tolerance_seconds)
    tests = Map.get(test_results, :tests, [])

    {matched, unmatched} =
      Enum.reduce(items, {[], []}, fn item, {matched_acc, unmatched_acc} ->
        case find_best_match(item, tests, tolerance) do
          {:ok, test, confidence} ->
            entry =
              %{module: test.module, test: test.name, confidence: confidence}
              |> Map.put(item_key, item)

            {[entry | matched_acc], unmatched_acc}

          :no_match ->
            {matched_acc, [item | unmatched_acc]}
        end
      end)

    %{matched: Enum.reverse(matched), unmatched: Enum.reverse(unmatched)}
  end

  @doc "Best confidence label from a list of confidence atoms."
  @spec confidence_label([confidence()]) :: String.t()
  def confidence_label(confidences) do
    cond do
      :high in confidences -> "high confidence"
      :low in confidences -> "low confidence"
      true -> ""
    end
  end

  @doc false
  def empty_result, do: %{matched: [], unmatched: []}

  defp find_best_match(%{timestamp: timestamp}, tests, tolerance) do
    result =
      Enum.reduce_while(tests, {:none, nil}, fn test, acc ->
        match_test_confidence(test, timestamp, tolerance, acc)
      end)

    case result do
      {:none, _} -> :no_match
      {confidence, test} -> {:ok, test, confidence}
    end
  end

  defp match_test_confidence(test, timestamp, tolerance, {best_conf, _} = acc) do
    case {test[:started_at], test[:finished_at]} do
      {%DateTime{} = started, %DateTime{} = finished} ->
        case calculate_confidence(timestamp, started, finished, tolerance) do
          :high -> {:halt, {:high, test}}
          :low when best_conf == :none -> {:cont, {:low, test}}
          _ -> {:cont, acc}
        end

      _ ->
        {:cont, acc}
    end
  end
end
