################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

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
      "invalid" => Map.get(counts, :invalid, 0),
      "invalidated" => Map.get(counts, :invalidated, 0)
    }
  end

  defp encode_test(test) do
    %{
      "module" => ToastTest.Formatting.display_module_name(test.module),
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
