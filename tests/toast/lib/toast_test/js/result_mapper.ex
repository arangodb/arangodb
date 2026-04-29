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

defmodule ToastTest.JS.ResultMapper do
  @moduledoc "Maps JSON results from JS test execution to ExUnit.Test/TestModule structs."

  @spec map_results(ToastTest.JS.Executor.result(), module(), String.t()) ::
          {ExUnit.TestModule.t(), [ExUnit.Test.t()]}
  def map_results(%{tests: test_results}, module_name, js_file) do
    tests = Enum.map(test_results, &map_test(&1, module_name, js_file))

    test_module = %ExUnit.TestModule{
      file: js_file,
      name: module_name,
      setup_all?: false,
      state: nil,
      tags: %{file: js_file, async: false, js_test: true},
      tests: tests
    }

    {test_module, tests}
  end

  defp map_test(result, module_name, js_file) do
    %ExUnit.Test{
      name: :"test #{result.name}",
      module: module_name,
      state: map_state(result.status, result.message),
      time: result.duration_ms * 1_000,
      tags: %{
        file: result.file || js_file,
        line: result.line,
        test_type: :test,
        started_at: result.started_at,
        finished_at: result.finished_at
      }
    }
  end

  defp map_state(:pass, _), do: nil
  defp map_state(:skip, nil), do: {:skipped, "skipped"}
  defp map_state(:skip, message), do: {:skipped, message}

  defp map_state(status, message) when status in [:fail, :error] do
    {:failed, [{:error, RuntimeError.exception(message || "unknown error"), []}]}
  end
end
