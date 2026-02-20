defmodule Toast.Diagnostics.SanitizerMatcher do
  @moduledoc """
  Match sanitizer errors to test cases using timestamps.

  After shutdown, compares sanitizer log file modification times against
  test execution windows (started_at..finished_at) to attribute errors
  to specific tests. Uses a two-tier confidence model:

  - **high**: file mtime falls within the test execution window
  - **low**: file mtime is within a tolerance window after test end
    (covers async sanitizer write delays)
  """

  @default_tolerance_seconds 5

  @type confidence :: :high | :low | :none

  @type match_entry :: %{
          module: atom(),
          test: String.t(),
          confidence: confidence(),
          error: Toast.Diagnostics.Sanitizer.sanitizer_error()
        }

  @type match_result :: %{
          matched: [match_entry()],
          unmatched: [Toast.Diagnostics.Sanitizer.sanitizer_error()]
        }

  @doc """
  Calculate match confidence between a sanitizer timestamp and a test window.

  Returns `:high` if the timestamp falls within `[test_start, test_end]`,
  `:low` if within `tolerance_seconds` after `test_end`, or `:none`.
  """
  @spec calculate_confidence(DateTime.t(), DateTime.t(), DateTime.t(), number()) :: confidence()
  def calculate_confidence(timestamp, test_start, test_end, tolerance_seconds \\ @default_tolerance_seconds) do
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
  Match sanitizer errors from diagnostics to test results by timestamp.

  Extracts all sanitizer errors from `diagnostics` (handles both single-server
  and cluster structures) and attempts to match each to a test case from
  `test_results`. Returns matched entries (grouped with test info and confidence)
  and unmatched errors.

  ## Options

    * `:tolerance_seconds` — seconds after test end for low-confidence match (default: 5)
  """
  @spec match(map() | nil, map() | nil, keyword()) :: match_result()
  def match(diagnostics, test_results, opts \\ [])

  def match(nil, _test_results, _opts), do: empty_result()
  def match(_diagnostics, nil, _opts), do: empty_result()

  def match(diagnostics, test_results, opts) do
    tolerance = Keyword.get(opts, :tolerance_seconds, @default_tolerance_seconds)
    errors = extract_all_errors(diagnostics)
    tests = Map.get(test_results, :tests, [])

    if errors == [] do
      empty_result()
    else
      do_match(errors, tests, tolerance)
    end
  end

  defp do_match(errors, tests, tolerance) do
    {matched, unmatched} =
      Enum.reduce(errors, {[], []}, fn error, {matched_acc, unmatched_acc} ->
        case find_best_match(error, tests, tolerance) do
          {:ok, test, confidence} ->
            entry = %{
              module: test.module,
              test: test.name,
              confidence: confidence,
              error: error
            }

            {[entry | matched_acc], unmatched_acc}

          :no_match ->
            {matched_acc, [error | unmatched_acc]}
        end
      end)

    %{matched: Enum.reverse(matched), unmatched: Enum.reverse(unmatched)}
  end

  defp find_best_match(%{timestamp: timestamp}, tests, tolerance) do
    result =
      Enum.reduce_while(tests, {:none, nil}, fn test, {best_conf, _best_test} = acc ->
        with %DateTime{} <- test[:started_at],
             %DateTime{} <- test[:finished_at] do
          case calculate_confidence(timestamp, test.started_at, test.finished_at, tolerance) do
            :high -> {:halt, {:high, test}}
            :low when best_conf == :none -> {:cont, {:low, test}}
            _ -> {:cont, acc}
          end
        else
          _ -> {:cont, acc}
        end
      end)

    case result do
      {:none, _} -> :no_match
      {confidence, test} -> {:ok, test, confidence}
    end
  end

  defp extract_all_errors(diagnostics) do
    if Toast.ResultExporter.cluster_diagnostics?(diagnostics) do
      Enum.flat_map(diagnostics, fn {_id, diag} -> Map.get(diag, :sanitizer_errors, []) end)
    else
      Map.get(diagnostics, :sanitizer_errors, [])
    end
  end

  defp empty_result, do: %{matched: [], unmatched: []}
end
