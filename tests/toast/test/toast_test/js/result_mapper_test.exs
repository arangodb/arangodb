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

defmodule ToastTest.JS.ResultMapperTest do
  use ExUnit.Case, async: true

  alias ToastTest.JS.ResultMapper

  @module_name :"Elixir.Fake.JS.SomeTest"

  describe "map_results/2" do
    test "maps passing test" do
      result = %{
        tests: [
          %{name: "testFoo", status: :pass, duration_ms: 100, message: nil, file: nil, line: nil}
        ]
      }

      {test_module, tests} = ResultMapper.map_results(result, @module_name, "some_test.js")

      assert %ExUnit.TestModule{name: @module_name} = test_module
      assert [%ExUnit.Test{} = test] = tests
      assert test.name == :"test testFoo"
      assert test.module == @module_name
      assert test.state == nil
      assert test.time == 100_000
    end

    test "maps failing test with message and location" do
      result = %{
        tests: [
          %{
            name: "testBar",
            status: :fail,
            duration_ms: 50,
            message: "Expected 3, got 2",
            file: "test_aql.js",
            line: 42
          }
        ]
      }

      {_test_module, [test]} = ResultMapper.map_results(result, @module_name, "some_test.js")

      assert {:failed, [{:error, exception, _stack}]} = test.state
      assert Exception.message(exception) =~ "Expected 3, got 2"
      assert test.tags.file == "test_aql.js"
      assert test.tags.line == 42
      assert test.time == 50_000
    end

    test "maps skipped test" do
      result = %{
        tests: [
          %{
            name: "testSkipped",
            status: :skip,
            duration_ms: 0,
            message: "not supported",
            file: nil,
            line: nil
          }
        ]
      }

      {_test_module, [test]} = ResultMapper.map_results(result, @module_name, "some_test.js")

      assert {:skipped, "not supported"} = test.state
    end

    test "maps error test" do
      result = %{
        tests: [
          %{
            name: "testCrash",
            status: :error,
            duration_ms: 10,
            message: "segfault",
            file: nil,
            line: nil
          }
        ]
      }

      {_test_module, [test]} = ResultMapper.map_results(result, @module_name, "some_test.js")

      assert {:failed, [{:error, exception, _stack}]} = test.state
      assert Exception.message(exception) =~ "segfault"
    end

    test "maps multiple tests" do
      result = %{
        tests: [
          %{name: "testA", status: :pass, duration_ms: 10, message: nil, file: nil, line: nil},
          %{name: "testB", status: :fail, duration_ms: 20, message: "boom", file: nil, line: nil},
          %{name: "testC", status: :skip, duration_ms: 0, message: nil, file: nil, line: nil}
        ]
      }

      {test_module, tests} = ResultMapper.map_results(result, @module_name, "some_test.js")

      assert length(tests) == 3
      assert test_module.tests == tests
      assert [nil, {:failed, _}, {:skipped, _}] = Enum.map(tests, & &1.state)
    end

    test "empty test list" do
      result = %{tests: []}

      {test_module, tests} = ResultMapper.map_results(result, @module_name, "some_test.js")

      assert tests == []
      assert test_module.tests == []
    end

    test "nil message on skip defaults to 'skipped'" do
      result = %{
        tests: [
          %{name: "testX", status: :skip, duration_ms: 0, message: nil, file: nil, line: nil}
        ]
      }

      {_test_module, [test]} = ResultMapper.map_results(result, @module_name, "some_test.js")

      assert {:skipped, "skipped"} = test.state
    end

    test "test tags include js file path when file not in result" do
      result = %{
        tests: [
          %{name: "testA", status: :pass, duration_ms: 0, message: nil, file: nil, line: nil}
        ]
      }

      {_test_module, [test]} = ResultMapper.map_results(result, @module_name, "some_test.js")

      assert test.tags.file == "some_test.js"
    end
  end
end
