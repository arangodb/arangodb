defmodule Toast.Diagnostics.LogMatcher do
  @moduledoc """
  Match server log warnings and assertion failures to test cases by timestamp.

  Follows the same pattern as `CrashMatcher` and `SanitizerMatcher`: extracts
  timestamped log entries from diagnostics and uses `Matcher` to attribute them
  to test cases whose execution window contains the entry's timestamp.
  """

  alias Toast.Diagnostics.Matcher

  @type log_entry :: %{
          server_id: String.t(),
          message: String.t(),
          kind: :warning | :assertion,
          timestamp: DateTime.t()
        }

  @type match_entry :: %{
          module: atom(),
          test: String.t(),
          confidence: Matcher.confidence(),
          log: log_entry()
        }

  @type match_result :: %{
          matched: [match_entry()],
          unmatched: [log_entry()]
        }

  @spec match(map() | nil, map() | nil, keyword()) :: match_result()
  def match(diagnostics, test_results, opts \\ [])

  def match(nil, _test_results, _opts), do: Matcher.empty_result()
  def match(_diagnostics, nil, _opts), do: Matcher.empty_result()

  def match(diagnostics, test_results, opts) do
    entries = extract_log_entries(diagnostics)
    {with_ts, without_ts} = Enum.split_with(entries, & &1.timestamp)
    result = Matcher.match(with_ts, test_results, :log, opts)
    %{result | unmatched: result.unmatched ++ without_ts}
  end

  defp extract_log_entries(diagnostics) do
    diagnostics
    |> Toast.Diagnostics.to_server_entries()
    |> Enum.flat_map(fn {server_id, diag} ->
      case Map.get(diag, :log_report) do
        nil ->
          []

        log ->
          tag_entries(log.warnings, server_id, :warning) ++
            tag_entries(log.assertion_failures, server_id, :assertion)
      end
    end)
  end

  defp tag_entries(entries, server_id, kind) do
    Enum.map(entries, fn entry ->
      %{
        server_id: server_id,
        message: entry.message,
        kind: kind,
        timestamp: entry.timestamp
      }
    end)
  end
end
