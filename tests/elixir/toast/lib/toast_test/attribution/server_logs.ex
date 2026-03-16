defmodule ToastTest.Attribution.ServerLogs do
  @moduledoc """
  Computes time windows from issues and extracts per-server log excerpts.

  Combines type-specific padding around issue timestamps, merges
  overlapping windows, then extracts matching log lines for each server.
  """

  alias ToastTest.Attribution.TimeWindows
  alias ToastTest.Enrichment

  @type window :: {DateTime.t(), DateTime.t()}

  @doc """
  Collect server log excerpts for all servers based on issue time windows.

  Returns `%{server_id => [{start, finish, lines}]}` where each tuple
  represents a merged time window and its extracted log lines.
  """
  @spec collect(
          [ToastTest.SuiteResult.issue()],
          ToastTest.ArtifactCollector.t(),
          TimeWindows.windows()
        ) :: %{String.t() => [{DateTime.t(), DateTime.t(), String.t()}]}
  def collect(issues, artifacts, windows) do
    merged = issues |> compute_windows(windows) |> merge_windows()

    if merged == [] do
      %{}
    else
      for {server_id, server_artifacts} <- artifacts,
          log_file = server_artifacts.server.log_file,
          log_file != nil,
          into: %{} do
        excerpts =
          for {start_dt, end_dt} <- merged,
              lines = Enrichment.Logs.extract_window(log_file, start_dt, end_dt),
              lines != "" do
            {start_dt, end_dt, lines}
          end

        {server_id, excerpts}
      end
    end
  end

  @doc """
  Compute padded time windows from a list of issues.

  Each issue type has different padding reflecting how much context
  is useful for diagnosing that kind of problem.
  """
  @spec compute_windows([ToastTest.SuiteResult.issue()], TimeWindows.windows()) :: [window()]
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
    |> Enum.sort_by(&elem(&1, 0), DateTime)
    |> do_merge([])
  end

  # --- Window computation per issue type ---

  defp issue_window(%{type: :test_failure, scope: {:test, mod, name}}, windows) do
    case Map.get(windows.tests, {mod, name}) do
      %{started_at: s, finished_at: f} -> [pad(s, f, -1, 1)]
      nil -> []
    end
  end

  defp issue_window(%{type: :test_failure}, _windows), do: []

  defp issue_window(%{type: :sanitizer_report, detail: %{report: _} = detail}, _windows) do
    # Sanitizer issues carry a timestamp from the file mtime, stored during
    # attribution. However the issue detail doesn't have it directly — the
    # timestamp was used for attribution and isn't stored on the issue.
    # We can't recover it here, so sanitizer reports without an explicit
    # timestamp in their detail are skipped.
    case Map.get(detail, :timestamp) do
      %DateTime{} = ts -> [pad(ts, ts, -5, 1)]
      nil -> []
    end
  end

  defp issue_window(%{type: :crash, detail: %{crash_info: %{timestamp: ts}}}, _windows)
       when not is_nil(ts) do
    [pad(ts, ts, -20, 0)]
  end

  defp issue_window(%{type: :crash}, _windows), do: []

  defp issue_window(%{type: :timeout, detail: %{timestamp: ts}}, _windows)
       when not is_nil(ts) do
    [pad(ts, ts, -10, 0)]
  end

  defp issue_window(%{type: :timeout}, _windows), do: []
  defp issue_window(_issue, _windows), do: []

  # --- Merge implementation ---

  defp do_merge([], acc), do: Enum.reverse(acc)

  defp do_merge([window | rest], []) do
    do_merge(rest, [window])
  end

  defp do_merge([{s2, f2} | rest], [{s1, f1} | merged]) do
    if DateTime.compare(s2, f1) != :gt do
      # Overlapping or adjacent — extend the current window
      merged_end = if DateTime.compare(f2, f1) == :gt, do: f2, else: f1
      do_merge(rest, [{s1, merged_end} | merged])
    else
      do_merge(rest, [{s2, f2}, {s1, f1} | merged])
    end
  end

  # --- Helpers ---

  defp pad(start_dt, end_dt, before_s, after_s) do
    {DateTime.add(start_dt, before_s, :second), DateTime.add(end_dt, after_s, :second)}
  end
end
