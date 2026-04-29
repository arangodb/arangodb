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

defmodule ToastTest.JS.TestRunner do
  @moduledoc """
  Runs a JavaScript test module by delegating to an `Executor` implementation,
  then emitting ExUnit-compatible events through the event pipeline.
  """

  alias ToastTest.JS.ResultMapper
  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.EventStore

  @spec run_module(Compat.event_manager(), module(), keyword()) :: :ok
  def run_module(manager, module, opts \\ []) do
    js_file = module.__toast_js_file__()
    executor = Keyword.fetch!(opts, :executor)
    executor_opts = Keyword.get(opts, :executor_opts, [])

    placeholder_module = module.__ex_unit__()
    EventStore.notify(%{event: :module_started, module: module})
    Compat.module_started(manager, placeholder_module)

    tests =
      case executor.run(js_file, executor_opts) do
        {:ok, result} ->
          {_test_module, tests} = ResultMapper.map_results(result, module, js_file)
          tests

        {:error, reason} ->
          [build_error_test(module, js_file, reason)]
      end

    for test <- tests do
      Compat.test_started(manager, test)
      Compat.test_finished(manager, test)
    end

    finished_module = %{placeholder_module | tests: tests}
    Compat.module_finished(manager, finished_module)
    EventStore.notify(%{event: :module_finished, module: module})
    :ok
  end

  defp build_error_test(module, js_file, reason) do
    %ExUnit.Test{
      name: :"test js_execution",
      module: module,
      state:
        {:failed,
         [{:error, RuntimeError.exception("JS executor failed: #{inspect(reason)}"), []}]},
      time: 0,
      tags: %{file: js_file, line: 0, test_type: :test}
    }
  end
end
