defmodule Toast.Diagnostics.SanitizerMatcher do
  @moduledoc """
  Match sanitizer errors to test cases using timestamps.

  After shutdown, compares sanitizer log file modification times against
  test execution windows (started_at..finished_at) to attribute errors
  to specific tests.
  """

  alias Toast.Diagnostics.Matcher

  @type match_entry :: %{
          module: atom(),
          test: String.t(),
          confidence: Matcher.confidence(),
          error: Toast.Diagnostics.Sanitizer.sanitizer_error()
        }

  @type match_result :: %{
          matched: [match_entry()],
          unmatched: [Toast.Diagnostics.Sanitizer.sanitizer_error()]
        }

  @doc """
  Match sanitizer errors from diagnostics to tests by timestamp.

  Extracts all sanitizer errors from `diagnostics` (handles both single-server
  and cluster structures) and attempts to match each to a test from `tests`.
  Returns matched entries (grouped with test info and confidence) and unmatched
  errors.

  ## Options

    * `:tolerance_seconds` — seconds after test end for low-confidence match (default: 5)
  """
  @spec match(map() | nil, [map()] | nil, keyword()) :: match_result()
  def match(diagnostics, tests, opts \\ [])

  def match(nil, _tests, _opts), do: Matcher.empty_result()
  def match(_diagnostics, nil, _opts), do: Matcher.empty_result()

  def match(diagnostics, tests, opts) do
    diagnostics
    |> extract_all_errors()
    |> Matcher.match(tests, :error, opts)
  end

  defp extract_all_errors(diagnostics) do
    diagnostics
    |> Toast.Diagnostics.to_server_entries()
    |> Enum.flat_map(fn {_id, diag} -> Map.get(diag, :sanitizer_errors, []) end)
  end
end
