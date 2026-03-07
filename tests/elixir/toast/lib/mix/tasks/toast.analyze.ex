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

    if opts[:failures] do
      format_failures(file_path)
    else
      results = decode_json!(file_path)
      output = format_output(results, opts)
      Mix.shell().info(output)
    end
  end

  defp format_failures(json_path) do
    etf_path = derive_etf_path(json_path)

    if File.exists?(etf_path) do
      failures =
        etf_path
        |> File.read!()
        |> :erlang.binary_to_term()

      ToastTest.CLIFormatter.print_failure_summary(failures)
    else
      results = decode_json!(json_path)
      Mix.shell().info(Toast.Analysis.Failures.format(results))
    end
  end

  defp derive_etf_path(json_path) do
    base = Path.rootname(json_path)
    Path.join(Path.dirname(json_path), Path.basename(base) <> ".failures.etf")
  end

  defp decode_json!(file_path) do
    content = File.read!(file_path)

    try do
      :json.decode(content)
    rescue
      _ -> Mix.raise("Error: invalid JSON in #{file_path}")
    end
  end

  defp format_output(results, opts) do
    cond do
      opts[:crashes] ->
        Toast.Analysis.Crashes.format(results)

      opts[:slow] ->
        Toast.Analysis.Performance.format(results, opts[:slow])

      true ->
        Toast.Analysis.Summary.format(results)
    end
  end
end
