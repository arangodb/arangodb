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

defmodule ToastTest.Runner.JS.ModuleBuilderTest do
  use ExUnit.Case, async: true

  alias ToastTest.Runner.JS.ModuleBuilder

  # A bare module atom — ModuleBuilder only stores it, doesn't call into it.
  @fake_suite ToastTest.Runner.JS.ModuleBuilderTest.FakeSuite

  describe "derive_module_name/2" do
    test "community file" do
      assert ModuleBuilder.derive_module_name(Smoke.Suite, "tests/js/client/aql/aql-parse.js") ==
               Smoke.JS.AqlParseTest
    end

    test "enterprise file gets Enterprise namespace" do
      path = "enterprise/tests/js/client/aql/aql-parse.js"

      assert ModuleBuilder.derive_module_name(Smoke.Suite, path) ==
               Smoke.JS.Enterprise.AqlParseTest
    end

    test "filename with underscores" do
      assert ModuleBuilder.derive_module_name(Smoke.Suite, "tests/js/test_aql_queries.js") ==
               Smoke.JS.AqlQueriesTest
    end

    test "suite with deep namespace" do
      assert ModuleBuilder.derive_module_name(
               Shell.Client.Aql.Suite,
               "tests/js/test_optimizer.js"
             ) ==
               Shell.Client.Aql.JS.OptimizerTest
    end
  end

  describe "build_module/3" do
    test "creates a module with expected functions" do
      suite = @fake_suite
      js_file = "/tmp/test_example.js"

      module = ModuleBuilder.build_module(suite, js_file)

      assert function_exported?(module, :__ex_unit__, 0)
      assert function_exported?(module, :__toast_suite__, 0)
      assert function_exported?(module, :__toast_js_file__, 0)
      assert function_exported?(module, :__toast_weight__, 0)
    end

    test "__toast_suite__ returns the suite module" do
      suite = @fake_suite
      js_file = "/tmp/test_foo.js"

      module = ModuleBuilder.build_module(suite, js_file)

      assert module.__toast_suite__() == suite
    end

    test "__toast_js_file__ returns the JS file path" do
      suite = @fake_suite
      js_file = "/tmp/test_bar.js"

      module = ModuleBuilder.build_module(suite, js_file)

      assert module.__toast_js_file__() == js_file
    end

    test "__toast_weight__ defaults to 1" do
      module = ModuleBuilder.build_module(@fake_suite, "/tmp/test_w.js")

      assert module.__toast_weight__() == 1
    end

    test "__ex_unit__ returns valid TestModule struct" do
      suite = @fake_suite
      js_file = "/tmp/test_meta.js"

      module = ModuleBuilder.build_module(suite, js_file)
      test_module = module.__ex_unit__()

      assert %ExUnit.TestModule{} = test_module
      assert test_module.name == String.to_atom(js_file)
      assert test_module.file == js_file
      assert test_module.setup_all? == false
      assert test_module.tags.async == false
    end

    test "__ex_unit__ includes a placeholder test" do
      module =
        ModuleBuilder.build_module(@fake_suite, "/tmp/test_placeholder.js")

      test_module = module.__ex_unit__()

      assert [%ExUnit.Test{} = test] = test_module.tests
      assert test.module == String.to_atom("/tmp/test_placeholder.js")
      assert test.tags.test_type == :test
    end

    test "build_module with custom weight" do
      module =
        ModuleBuilder.build_module(@fake_suite, "/tmp/test_heavy.js", weight: 5)

      assert module.__toast_weight__() == 5
    end
  end

  describe "tags_from_filename/1" do
    test "cluster-only file" do
      assert ModuleBuilder.tags_from_filename("aql-join-cluster.js") == %{cluster_only: true}
    end

    test "noncluster file gets single_only, not cluster_only" do
      tags = ModuleBuilder.tags_from_filename("aql-join-noncluster.js")
      assert tags == %{single_only: true}
      refute Map.has_key?(tags, :cluster_only)
    end

    test "multiple tags" do
      tags = ModuleBuilder.tags_from_filename("aql-nightly-cluster.js")
      assert tags == %{cluster_only: true, full_only: true}
    end

    test "no matching segments" do
      assert ModuleBuilder.tags_from_filename("aql-parse.js") == %{}
    end

    test "failure points" do
      assert ModuleBuilder.tags_from_filename("aql-crash-fp.js") == %{failure_points: true}
    end

    test "skip_sanitizer" do
      assert ModuleBuilder.tags_from_filename("test-noasan.js") == %{skip_sanitizer: true}
    end

    test "skip_instrumented" do
      assert ModuleBuilder.tags_from_filename("test-noinstr.js") == %{skip_instrumented: true}
    end

    test "replication2" do
      assert ModuleBuilder.tags_from_filename("test-r2.js") == %{replication2: true}
    end

    test "server_javascript" do
      assert ModuleBuilder.tags_from_filename("test-sjs.js") == %{server_javascript: true}
    end
  end

  describe "build_modules/3" do
    test "creates a module for each JS file" do
      suite = @fake_suite
      files = ["/tmp/test_a.js", "/tmp/test_b.js"]

      modules = ModuleBuilder.build_modules(suite, files)

      assert length(modules) == 2
      assert Enum.all?(modules, &function_exported?(&1, :__toast_js_file__, 0))

      js_files = Enum.map(modules, & &1.__toast_js_file__())
      assert "/tmp/test_a.js" in js_files
      assert "/tmp/test_b.js" in js_files
    end

    test "applies per-file weights from weights map" do
      suite = @fake_suite
      files = ["/tmp/test_light.js", "/tmp/test_heavy.js"]

      modules =
        ModuleBuilder.build_modules(suite, files, weights: %{"test_heavy.js" => 10})

      weights = Map.new(modules, fn mod -> {mod.__toast_js_file__(), mod.__toast_weight__()} end)
      assert weights["/tmp/test_light.js"] == 1
      assert weights["/tmp/test_heavy.js"] == 10
    end
  end
end
