defmodule Toast.Diagnostics.CrashMatcher do
  @moduledoc """
  Match server crashes to test cases using timestamps.

  After shutdown, extracts the timestamp from each server's crash report
  (the first `{crash}` FATAL log line) and compares it against test execution
  windows (started_at..finished_at) to attribute crashes to specific tests.
  """

  alias Toast.Diagnostics.Matcher

  @type crash_info :: %{
          server_id: String.t(),
          signal_name: String.t(),
          signal_number: non_neg_integer() | nil,
          crash_header: String.t() | nil,
          backtrace: [String.t()],
          fatal_lines: [String.t()],
          crash_output: [String.t()],
          log_file: String.t() | nil,
          timestamp: DateTime.t()
        }

  @type match_entry :: %{
          module: atom(),
          test: String.t(),
          confidence: Matcher.confidence(),
          crash: crash_info()
        }

  @type match_result :: %{
          matched: [match_entry()],
          unmatched: [crash_info()]
        }

  @doc """
  Match crash reports from diagnostics to test results by timestamp.

  Extracts crash reports from `diagnostics` (handles both single-server
  and cluster structures), filters to those with a signal and timestamp,
  and attempts to match each to a test case. Returns matched entries
  (with test info and confidence) and unmatched crashes.

  ## Options

    * `:tolerance_seconds` — seconds after test end for low-confidence match (default: 5)
  """
  @spec match(map() | nil, map() | nil, keyword()) :: match_result()
  def match(diagnostics, test_results, opts \\ [])

  def match(nil, _test_results, _opts), do: Matcher.empty_result()
  def match(_diagnostics, nil, _opts), do: Matcher.empty_result()

  def match(diagnostics, test_results, opts) do
    crashes = extract_crashes(diagnostics)
    {with_ts, without_ts} = Enum.split_with(crashes, & &1.timestamp)
    result = Matcher.match(with_ts, test_results, :crash, opts)
    %{result | unmatched: result.unmatched ++ without_ts}
  end

  defp extract_crashes(diagnostics) do
    diagnostics
    |> Toast.Diagnostics.to_server_entries()
    |> Enum.flat_map(fn {_id, diag} -> maybe_crash_info(diag) end)
  end

  defp maybe_crash_info(diag) do
    crash = Map.get(diag, :log_report)
    server = Map.get(diag, :server)

    if crash && crash.signal_name do
      [
        %{
          server_id: server && server.id,
          signal_name: crash.signal_name,
          signal_number: crash.signal_number,
          crash_header: crash.crash_header,
          backtrace: crash.backtrace,
          fatal_lines: crash.fatal_lines,
          crash_output: crash.crash_output,
          log_file: server && server.log_file,
          timestamp: crash.timestamp
        }
      ]
    else
      []
    end
  end
end
