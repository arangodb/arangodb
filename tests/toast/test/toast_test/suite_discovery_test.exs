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

defmodule ToastTest.SuiteDiscoveryTest do
  use ExUnit.Case, async: true

  describe "modules without __toast_suite__/0 are not recognized" do
    test "plain module does not export __toast_suite__/0" do
      defmodule PlainModule do
        def hello, do: :world
      end

      refute function_exported?(PlainModule, :__toast_suite__, 0)
    end

    test "plain ExUnit test module does not export __toast_suite__/0" do
      defmodule PlainTestModule do
        use ExUnit.Case
      end

      refute function_exported?(PlainTestModule, :__toast_suite__, 0)
    end
  end

  describe "suite compilation ordering" do
    test "suite.ex defines the suite module before test files use it" do
      # Suite modules must be compiled first so test modules can `use` them.
      # The mix task compiles suite.ex files via ParallelCompiler.compile,
      # then helpers (.ex), then test files (.exs) via ParallelCompiler.require.
      # We verify this ordering by checking that the suite module's __using__
      # macro is available before test modules reference it.

      defmodule OrderedSuite do
        use ToastTest.Suite, mode: :single_server
      end

      # Suite is fully compiled and its macro is available
      assert macro_exported?(OrderedSuite, :__using__, 1)

      # Now test modules can use it
      defmodule OrderedTestModule do
        use ToastTest.SuiteDiscoveryTest.OrderedSuite
      end

      assert OrderedTestModule.__toast_suite__() == OrderedSuite
    end
  end
end
