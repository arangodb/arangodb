defmodule ToastTest.ResultExporter do
  @moduledoc "Export test results and diagnostics to structured file formats."

  require Logger

  alias ToastTest.ResultExporter.{JSON, JUnitXML}

  defmodule AnalysisData do
    @moduledoc "Bundled analysis results passed through the export pipeline."

    @type t :: %__MODULE__{
            diagnostics: map() | nil,
            sanitizer_matching: map() | nil,
            crash_matching: map() | nil,
            log_matching: map() | nil
          }

    defstruct diagnostics: nil,
              sanitizer_matching: nil,
              crash_matching: nil,
              log_matching: nil
  end

  @default_result_dir "toast-results"

  @doc """
  Export test results to JSON and JUnit XML files.

  Writes files to the directory specified by TOAST_RESULT_DIR env var,
  defaulting to `toast-results` in the current working directory.

  No-op if results is nil.
  """
  @spec export(String.t(), map() | nil, AnalysisData.t() | nil) :: :ok
  def export(suite_name, results, analysis \\ %AnalysisData{})

  def export(_suite_name, nil, _analysis) do
    Logger.info("No results!")
    :ok
  end

  def export(suite_name, results, %AnalysisData{} = analysis) do
    result_dir = result_dir()
    File.mkdir_p!(result_dir)

    json_path = Path.join(result_dir, "#{suite_name}.json")
    File.write!(json_path, JSON.render(results, analysis))

    xml_path = Path.join(result_dir, "#{suite_name}.xml")
    File.write!(xml_path, JUnitXML.render(results, analysis))

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
end
