defmodule ToastTest.ResultExporter do
  @moduledoc "Export test results and diagnostics to structured file formats."

  require Logger

  alias ToastTest.ResultExporter.{JSON, JUnitXML}

  @results_key :__test_results__
  @diagnostics_key :__test_diagnostics__
  @sanitizer_matching_key :__sanitizer_matching__
  @crash_matching_key :__crash_matching__
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
    sanitizer_matching = Application.get_env(:toast, @sanitizer_matching_key)
    crash_matching = Application.get_env(:toast, @crash_matching_key)

    if results do
      File.mkdir_p!(result_dir)

      json_path = Path.join(result_dir, "results.json")
      File.write!(json_path, JSON.render(results, diagnostics, sanitizer_matching, crash_matching))

      xml_path = Path.join(result_dir, "results.xml")
      File.write!(xml_path, JUnitXML.render(results, diagnostics, sanitizer_matching, crash_matching))

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
  defdelegate cluster_diagnostics?(diagnostics), to: Toast.Diagnostics
end
