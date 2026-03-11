defmodule ToastTest.SuiteResult.JSON do
  @moduledoc false

  alias ToastTest.SuiteResult

  @spec write(SuiteResult.t(), Path.t()) :: :ok
  def write(%SuiteResult{} = result, result_dir) do
    tests = SuiteResult.flat_tests(result)

    data = %{
      "suite" => result.suite,
      "started_at" => DateTime.to_iso8601(result.started_at),
      "finished_at" => if(result.finished_at, do: DateTime.to_iso8601(result.finished_at)),
      "duration_us" => result.times_us.run,
      "summary" => build_summary(tests),
      "tests" => Enum.map(tests, &encode_test/1)
    }

    json = data |> :json.format(&json_encoder/3, %{}) |> IO.iodata_to_binary()
    File.write!(Path.join(result_dir, "outcomes.json"), json)
    :ok
  end

  defp build_summary(tests) do
    counts = Enum.frequencies_by(tests, & &1.outcome)

    %{
      "passed" => Map.get(counts, :passed, 0),
      "failed" => Map.get(counts, :failed, 0),
      "skipped" => Map.get(counts, :skipped, 0),
      "excluded" => Map.get(counts, :excluded, 0),
      "invalid" => Map.get(counts, :invalid, 0)
    }
  end

  defp encode_test(test) do
    %{
      "module" => Atom.to_string(test.module),
      "name" => Atom.to_string(test.name),
      "outcome" => Atom.to_string(test.outcome),
      "duration_us" => test.duration_us
    }
  end

  defp json_encoder(nil, _encoder, _opts), do: "null"

  defp json_encoder(value, encoder, opts) when is_atom(value) and not is_boolean(value) do
    value |> Atom.to_string() |> encoder.(encoder, opts)
  end

  defp json_encoder(value, encoder, opts), do: :json.format_value(value, encoder, opts)
end
