defmodule Toast.DiagnosticsTestHelpers do
  @base_time ~U[2024-06-15 10:00:00Z]

  def base_time, do: @base_time

  def at(seconds), do: DateTime.add(@base_time, seconds, :second)

  def make_test(opts \\ []) do
    %{
      module: Keyword.get(opts, :module, SmokeTest.VersionTest),
      name: Keyword.get(opts, :name, "test server version"),
      outcome: :passed,
      duration_us: 1_000_000,
      failure: nil,
      started_at: Keyword.get(opts, :started_at, at(0)),
      finished_at: Keyword.get(opts, :finished_at, at(10)),
      tags: %{file: "test/version_test.exs", line: 5}
    }
  end

  def make_test_results(tests) do
    %{
      suite_started_at: @base_time,
      suite_finished_at: at(60),
      times_us: %{run: 60_000_000, async: nil, load: 100_000},
      tests: tests
    }
  end
end
