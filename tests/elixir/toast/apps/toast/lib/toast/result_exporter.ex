defmodule Toast.ResultExporter do
  @moduledoc "Export test results and diagnostics to structured file formats."

  require Logger

  alias Toast.ResultExporter.{JSON, JUnitXML}

  @results_key :__test_results__
  @diagnostics_key :__test_diagnostics__
  @default_result_dir "toast-results"

  @doc """
  Export test results to JSON and JUnit XML files.

  Reads results and diagnostics from Application env (populated by
  ResultFormatter and TestCase after_suite callback respectively).
  Writes files to the directory specified by TOAST_RESULT_DIR env var,
  defaulting to `toast-results` in the current working directory.

  No-op if no results were collected.
  """
  @spec export() :: :ok
  def export do
    do_export(result_dir())
  end

  @doc false
  @spec result_dir() :: Path.t()
  def result_dir do
    System.get_env("TOAST_RESULT_DIR") || @default_result_dir
  end

  defp do_export(result_dir) do
    results = Application.get_env(:toast, @results_key)
    diagnostics = Application.get_env(:toast, @diagnostics_key)

    if results do
      File.mkdir_p!(result_dir)

      json_path = Path.join(result_dir, "results.json")
      File.write!(json_path, JSON.render(results, diagnostics))

      xml_path = Path.join(result_dir, "results.xml")
      File.write!(xml_path, JUnitXML.render(results, diagnostics))

      Logger.info("Results written to #{result_dir}")
    else
      Logger.info("No results!")
    end

    :ok
  rescue
    error ->
      Logger.warning("Failed to export: #{Exception.message(error)}")
      :ok
  end

  @doc false
  @spec cluster_diagnostics?(map()) :: boolean()
  def cluster_diagnostics?(diagnostics) do
    case Map.keys(diagnostics) do
      [] ->
        false

      [first_key | _] ->
        is_binary(first_key) and is_map(Map.get(diagnostics, first_key)) and
          Map.has_key?(Map.get(diagnostics, first_key), :sanitizer_errors)
    end
  end
end
