defmodule Mix.Tasks.Toast.Analyze do
  @shortdoc "Analyze Toast test results"
  @moduledoc """
  Post-run analysis of Toast test results.

  ## Usage

      mix toast.analyze <results.json> [options]

  ## Options

      --failures    Show detailed failure info with stack traces
      --crashes     Show crash diagnostics, sanitizer errors, coredump traces
      --slow N      Show N slowest tests with durations (default: 10)
  """

  use Mix.Task

  @switches [
    failures: :boolean,
    crashes: :boolean,
    slow: :integer
  ]

  @impl Mix.Task
  def run(args) do
    {opts, rest} = OptionParser.parse!(args, strict: @switches)

    file_path =
      case rest do
        [path | _] ->
          path

        [] ->
          Mix.raise("Usage: mix toast.analyze <results.json> [--failures] [--crashes] [--slow N]")
      end

    unless File.exists?(file_path) do
      Mix.raise("Error: file not found: #{file_path}")
    end

    content = File.read!(file_path)

    results =
      try do
        :json.decode(content)
      rescue
        _ -> Mix.raise("Error: invalid JSON in #{file_path}")
      end

    output = format_output(results, opts)
    Mix.shell().info(output)
  end

  defp format_output(results, opts) do
    cond do
      opts[:failures] ->
        Toast.Analysis.Failures.format(results)

      opts[:crashes] ->
        Toast.Analysis.Crashes.format(results)

      opts[:slow] ->
        Toast.Analysis.Performance.format(results, opts[:slow])

      true ->
        Toast.Analysis.Summary.format(results)
    end
  end
end
