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
            log_matching: map() | nil,
            failures: [ExUnit.Test.t()]
          }

    defstruct diagnostics: nil,
              sanitizer_matching: nil,
              crash_matching: nil,
              log_matching: nil,
              failures: []
  end

  @spec export(String.t(), map() | nil, AnalysisData.t() | nil, Path.t()) :: :ok
  def export(suite_name, results, analysis \\ %AnalysisData{}, result_dir)

  def export(_suite_name, nil, _analysis, _result_dir) do
    Logger.info("No results!")
    :ok
  end

  def export(suite_name, results, %AnalysisData{} = analysis, result_dir) do
    File.mkdir_p!(result_dir)

    json_path = Path.join(result_dir, "#{suite_name}.json")
    File.write!(json_path, JSON.render(results, analysis))

    xml_path = Path.join(result_dir, "#{suite_name}.xml")
    File.write!(xml_path, JUnitXML.render(results, analysis))

    if analysis.failures != [] do
      etf_path = Path.join(result_dir, "#{suite_name}.failures.etf")
      File.write!(etf_path, :erlang.term_to_binary(analysis.failures))
    end

    Logger.info("Results written to #{result_dir}")
    :ok
  rescue
    error ->
      Logger.warning("Failed to export: #{Exception.message(error)}")
      :ok
  end
end
