defmodule ToastTest.Attribution.ServerLogs do
  @moduledoc """
  Computes time windows from issues and extracts per-server log excerpts.

  Combines type-specific padding around issue timestamps, merges
  overlapping windows, then extracts matching log lines for each server.
  """

  alias ToastTest.Attribution.TimeWindows
  alias ToastTest.Enrichment

  @type window :: {Toast.timestamp(), Toast.timestamp()}

  @doc """
  Collect server log excerpts for all servers based on issue time windows.

  `log_files` is `%{server_id => log_file_path}` — a flat map of all servers
  whose logs should be collected, regardless of which deployment they belong to.

  Returns `%{server_id => [{start, finish, entries}]}` where each tuple
  represents a merged time window and its extracted log entries.
  """
  @spec collect(
          [ToastTest.SuiteResult.issue()],
          %{String.t() => Path.t()},
          ToastTest.Attribution.TimeWindows.windows()
        ) :: %{String.t() => [{Toast.timestamp(), Toast.timestamp(), [map()]}]}
  def collect(issues, log_files, windows) do
    issues
    |> compute_windows(windows)
    |> merge_windows()
    |> do_collect(log_files)
  end

  defp do_collect([], _), do: %{}

  defp do_collect(merged, log_files) do
    for {server_id, log_file} <- log_files, into: %{} do
      excerpts =
        merged
        |> Enrichment.Logs.extract_windows(log_file)
        |> Enum.zip(merged)
        |> Enum.flat_map(fn
          {[], _window} -> []
          {entries, {start_us, end_us}} -> [{start_us, end_us, entries}]
        end)

      {server_id, excerpts}
    end
  end

  @doc """
  Compute padded time windows from a list of issues.

  Each issue type has different padding reflecting how much context
  is useful for diagnosing that kind of problem.
  """
  @spec compute_windows(
          [ToastTest.SuiteResult.issue()],
          ToastTest.Attribution.TimeWindows.windows()
        ) ::
          [window()]
  def compute_windows(issues, windows) do
    Enum.flat_map(issues, &issue_window(&1, windows))
  end

  @doc """
  Merge overlapping or adjacent time windows into a minimal set.

  Windows are sorted by start time, then any pair where the second
  starts at or before the first ends is merged into a single window.
  """
  @spec merge_windows([window()]) :: [window()]
  def merge_windows([]), do: []

  def merge_windows(windows) do
    windows
    |> Enum.sort_by(&elem(&1, 0))
    |> do_merge([])
  end

  # --- Window computation per issue type ---

  defp issue_window(%{type: :test_failure, scope: {:test, mod, name}}, windows) do
    case Map.get(windows.tests, {mod, name}) do
      %{started_at: s, finished_at: f} -> [TimeWindows.pad(s, f, :test_failure)]
      nil -> []
    end
  end

  defp issue_window(%{type: :test_failure}, _windows), do: []

  defp issue_window(%{type: :sanitizer_report, detail: %{timestamp: ts}}, _windows)
       when is_integer(ts) do
    [TimeWindows.pad(ts, ts, :sanitizer)]
  end

  defp issue_window(%{type: :sanitizer_report}, _windows), do: []

  defp issue_window(%{type: :crash, detail: %{crash_info: %{timestamp: ts}}}, _windows)
       when is_integer(ts) do
    [TimeWindows.pad(ts, ts, :crash)]
  end

  defp issue_window(%{type: :crash}, _windows), do: []

  defp issue_window(%{type: :timeout, detail: %{timestamp: ts}}, _windows)
       when is_integer(ts) do
    [TimeWindows.pad(ts, ts, :timeout)]
  end

  defp issue_window(%{type: :timeout}, _windows), do: []
  defp issue_window(_issue, _windows), do: []

  # --- Merge implementation ---

  defp do_merge([], acc), do: Enum.reverse(acc)

  defp do_merge([window | rest], []) do
    do_merge(rest, [window])
  end

  defp do_merge([{s2, f2} | rest], [{s1, f1} | merged]) do
    if s2 <= f1 do
      do_merge(rest, [{s1, max(f1, f2)} | merged])
    else
      do_merge(rest, [{s2, f2}, {s1, f1} | merged])
    end
  end
end
