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

defmodule ToastTest.Runner.JS.TestRunner do
  @moduledoc """
  Runs a JavaScript test module by delegating to an `Executor` implementation,
  then emitting ExUnit-compatible events through the event pipeline.
  """

  alias ToastTest.Runner.{Events, FailureFormatter}
  alias ToastTest.Runner.JS.ResultMapper
  alias ToastTest.ExUnitCompat, as: Compat

  @spec run_module(Compat.event_manager(), module(), keyword()) :: :ok
  def run_module(manager, module, opts \\ []) do
    js_file = module.__toast_js_file__()
    executor = Keyword.fetch!(opts, :executor)
    executor_opts = Keyword.get(opts, :executor_opts, [])

    placeholder_module = module.__ex_unit__()
    js_module = placeholder_module.name
    Events.module_started(manager, module, placeholder_module)

    tests =
      case executor.run(js_file, executor_opts) do
        {:ok, result} ->
          ResultMapper.map_results(result, js_module, js_file)

        {:error, reason} ->
          [build_error_test(js_module, js_file, reason)]
      end

    # We are emitting these events after all tests have run, which means the timestamps won't reflect
    # the actual start and finish times. We need to do that to make sure we have all the events in the
    # event store and that the RunnerStats are updated correctly.
    for test <- tests do
      Events.test_started(manager, test, timestamp_from_tag(test, :started_at))
      Events.test_finished(manager, test, timestamp_from_tag(test, :finished_at))
    end

    finished_module = %{placeholder_module | tests: tests}
    Events.module_finished(manager, module, finished_module)
    :ok
  end

  defp timestamp_from_tag(test, key) do
    case Map.get(test.tags, key) do
      %DateTime{} = dt -> [timestamp: DateTime.to_unix(dt, :microsecond)]
      _ -> []
    end
  end

  defp build_error_test(js_module, js_file, reason) do
    %ExUnit.Test{
      name: :"test js_execution",
      module: js_module,
      state:
        FailureFormatter.failed(
          :error,
          RuntimeError.exception("JS executor failed: #{inspect(reason)}"),
          []
        ),
      time: 0,
      tags: %{file: js_file, line: 0, test_type: :test}
    }
  end
end
