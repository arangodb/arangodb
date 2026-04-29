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

defmodule ToastTest.JS.TestRunnerTest do
  use ExUnit.Case, async: true

  alias ToastTest.JS.{ModuleBuilder, TestRunner}
  alias ToastTest.ExUnitCompat, as: Compat

  defmodule PassingExecutor do
    @behaviour ToastTest.JS.Executor

    @impl true
    def run(_js_file, _opts) do
      {:ok,
       %{
         tests: [
           %{
             name: "testFoo",
             status: :pass,
             duration_ms: 100,
             message: nil,
             file: nil,
             line: nil,
             started_at: nil,
             finished_at: nil
           },
           %{
             name: "testBar",
             status: :pass,
             duration_ms: 200,
             message: nil,
             file: nil,
             line: nil,
             started_at: nil,
             finished_at: nil
           }
         ]
       }}
    end
  end

  defmodule FailingExecutor do
    @behaviour ToastTest.JS.Executor

    @impl true
    def run(_js_file, _opts) do
      {:ok,
       %{
         tests: [
           %{
             name: "testGood",
             status: :pass,
             duration_ms: 50,
             message: nil,
             file: nil,
             line: nil,
             started_at: nil,
             finished_at: nil
           },
           %{
             name: "testBad",
             status: :fail,
             duration_ms: 75,
             message: "assertion failed",
             file: "test_thing.js",
             line: 42,
             started_at: nil,
             finished_at: nil
           }
         ]
       }}
    end
  end

  defmodule ErrorExecutor do
    @behaviour ToastTest.JS.Executor

    @impl true
    def run(_js_file, _opts) do
      {:error, "arangosh crashed with exit code 1"}
    end
  end

  setup do
    {:ok, manager} = Compat.start_event_manager()
    {:ok, stats_pid} = Compat.add_runner_stats(manager, [])
    {:ok, collector_pid} = Compat.add_formatter(manager, ToastTest.ResultCollector, [])

    Compat.suite_started(manager, [])

    %{manager: manager, stats_pid: stats_pid, collector_pid: collector_pid}
  end

  defp build_js_module(suite_name_suffix) do
    # Each test needs a unique suite to get a unique module name.
    suite = Module.concat([ToastTest.JS.TestRunnerTest, "Suite#{suite_name_suffix}"])
    ModuleBuilder.build_module(suite, "/tmp/test_example_#{suite_name_suffix}.js")
  end

  describe "run_module/3" do
    test "emits events for passing tests", ctx do
      module = build_js_module("Pass")

      TestRunner.run_module(ctx.manager, module, executor: PassingExecutor)

      Compat.suite_finished(ctx.manager, %{async: nil, load: nil, run: 0})
      test_data = ToastTest.ResultCollector.get_data(ctx.collector_pid)

      assert test_data.modules[module]
      tests = test_data.modules[module].tests
      assert length(tests) == 2
      assert Enum.all?(tests, &(&1.outcome == :passed))
    end

    test "emits events for failing tests", ctx do
      module = build_js_module("Fail")

      TestRunner.run_module(ctx.manager, module, executor: FailingExecutor)

      Compat.suite_finished(ctx.manager, %{async: nil, load: nil, run: 0})
      test_data = ToastTest.ResultCollector.get_data(ctx.collector_pid)

      tests = test_data.modules[module].tests
      outcomes = Enum.map(tests, & &1.outcome)
      assert :passed in outcomes
      assert :failed in outcomes
    end

    test "handles executor error by failing the module", ctx do
      module = build_js_module("Error")

      TestRunner.run_module(ctx.manager, module, executor: ErrorExecutor)

      Compat.suite_finished(ctx.manager, %{async: nil, load: nil, run: 0})
      test_data = ToastTest.ResultCollector.get_data(ctx.collector_pid)

      tests = test_data.modules[module].tests
      assert length(tests) == 1
      assert [%{outcome: :failed}] = tests
    end
  end
end
