defmodule ToastTest.ResultExporter do
  @moduledoc "Export test results and diagnostics to structured file formats."

  require Logger

  alias ToastTest.ResultExporter.{JSON, JUnitXML}

  @default_result_dir "toast-results"

  @doc """
  Export test results to JSON and JUnit XML files.

  Writes files to the directory specified by TOAST_RESULT_DIR env var,
  defaulting to `toast-results` in the current working directory.

  No-op if results is nil.
  """
  @spec export(map() | nil, map() | nil, map() | nil, map() | nil, map() | nil) :: :ok
  def export(
        results,
        diagnostics \\ nil,
        sanitizer_matching \\ nil,
        crash_matching \\ nil,
        log_matching \\ nil
      )

  def export(nil, _diagnostics, _sanitizer_matching, _crash_matching, _log_matching) do
    Logger.info("No results!")
    :ok
  end

  def export(results, diagnostics, sanitizer_matching, crash_matching, log_matching) do
    result_dir = result_dir()
    File.mkdir_p!(result_dir)

    json_path = Path.join(result_dir, "results.json")

    File.write!(
      json_path,
      JSON.render(results, diagnostics, sanitizer_matching, crash_matching, log_matching)
    )

    xml_path = Path.join(result_dir, "results.xml")

    File.write!(
      xml_path,
      JUnitXML.render(results, diagnostics, sanitizer_matching, crash_matching, log_matching)
    )

    Logger.info("Results written to #{result_dir}")
    :ok
  rescue
    error ->
      Logger.warning("Failed to export: #{Exception.message(error)}")
      :ok
  end

  @doc false
  @spec result_dir() :: Path.t()
  def result_dir do
    System.get_env("TOAST_RESULT_DIR") || @default_result_dir
  end

  @doc """
  Export suite-level results to JSON and JUnit XML files.
  Called by the runner after all suites complete.
  """
  @spec export_suites(map()) :: :ok
  def export_suites(suite_results) do
    result_dir = result_dir()
    File.mkdir_p!(result_dir)

    json_path = Path.join(result_dir, "results.json")
    File.write!(json_path, JSON.render_suites(suite_results))

    xml_path = Path.join(result_dir, "results.xml")
    File.write!(xml_path, JUnitXML.render_suites(suite_results))

    Logger.info("Results written to #{result_dir}")
    :ok
  rescue
    error ->
      Logger.warning("Failed to export: #{Exception.message(error)}")
      :ok
  end

end
