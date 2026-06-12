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

defmodule ToastTest.Interactive.ResolverTest do
  use ExUnit.Case, async: false

  alias ToastTest.Interactive.Resolver

  defmodule Fixtures.Suite do
    use ToastTest.Suite
  end

  defmodule Fixtures.SuiteTest do
    use ExUnit.Case, async: false
    def __toast_suite__, do: Fixtures.Suite
    test "ok", do: assert(true)
  end

  describe "resolve/1 with atoms" do
    test "resolves a suite module to its test modules" do
      assert {Fixtures.Suite, test_modules} = Resolver.resolve(Fixtures.Suite)
      assert Fixtures.SuiteTest in test_modules
    end

    test "resolves a test module to its suite" do
      assert {Fixtures.Suite, [Fixtures.SuiteTest]} = Resolver.resolve(Fixtures.SuiteTest)
    end

    test "raises for a module without a suite" do
      defmodule Standalone do
        use ExUnit.Case, async: false
        test "standalone", do: assert(true)
      end

      assert_raise ArgumentError, ~r/does not belong to a suite/, fn ->
        Resolver.resolve(Standalone)
      end
    end

    test "raises for an unloaded module" do
      assert_raise ArgumentError, ~r/is not loaded/, fn ->
        Resolver.resolve(Nonexistent.Nothing)
      end
    end

    @tag :tmp_dir
    test "resolves a test module via its suite directory", %{tmp_dir: dir} do
      {suite_dir, uid} = create_fixture(dir)
      {suite_module, _test_modules} = Resolver.compile_all(suite_dir)
      test_module = :"Elixir.ResolverFixture#{uid}.ExampleTest"

      assert {^suite_module, [^test_module]} = Resolver.resolve(test_module)
    end
  end

  describe "find_suite_root!/1" do
    @tag :tmp_dir
    test "returns dir when it contains suite.ex", %{tmp_dir: dir} do
      File.write!(Path.join(dir, "suite.ex"), "")
      assert Resolver.find_suite_root!(dir) == dir
    end

    @tag :tmp_dir
    test "walks up to parent containing suite.ex", %{tmp_dir: dir} do
      File.write!(Path.join(dir, "suite.ex"), "")
      child = Path.join(dir, "sub")
      File.mkdir_p!(child)
      assert Resolver.find_suite_root!(child) == dir
    end

    @tag :tmp_dir
    test "walks up multiple levels", %{tmp_dir: dir} do
      File.write!(Path.join(dir, "suite.ex"), "")
      deep = Path.join([dir, "a", "b", "c"])
      File.mkdir_p!(deep)
      assert Resolver.find_suite_root!(deep) == dir
    end

    @tag :tmp_dir
    test "handles file paths by using parent directory", %{tmp_dir: dir} do
      File.write!(Path.join(dir, "suite.ex"), "")
      file = Path.join(dir, "test_foo.exs")
      File.write!(file, "")
      assert Resolver.find_suite_root!(file) == dir
    end

    @tag :tmp_dir
    test "raises with original path in message when no suite.ex found", %{tmp_dir: dir} do
      child = Path.join(dir, "no_suite_here")
      File.mkdir_p!(child)

      assert_raise ArgumentError, ~r/no suite\.ex found in #{Regex.escape(child)}/, fn ->
        Resolver.find_suite_root!(child)
      end
    end
  end

  describe "compile_all/1" do
    @tag :tmp_dir
    test "returns suite module and test modules", %{tmp_dir: dir} do
      {suite_dir, uid} = create_fixture(dir)
      {suite_module, test_modules} = Resolver.compile_all(suite_dir)
      assert suite_module == :"Elixir.ResolverFixture#{uid}.Suite"
      assert length(test_modules) == 1
    end

    @tag :tmp_dir
    test "raises when no suite.ex in dir", %{tmp_dir: dir} do
      assert_raise ArgumentError, ~r/no suite\.ex found/, fn ->
        Resolver.compile_all(dir)
      end
    end

    @tag :tmp_dir
    test "raises when no test files in dir", %{tmp_dir: dir} do
      uid = System.unique_integer([:positive])

      File.write!(Path.join(dir, "suite.ex"), """
      defmodule ResolverFixture#{uid}.EmptySuite do
        use ToastTest.Suite
      end
      """)

      assert_raise ArgumentError, ~r/no test modules found/, fn ->
        Resolver.compile_all(dir)
      end
    end
  end

  describe "compile_suite/1" do
    @tag :tmp_dir
    test "compiles helper .ex files alongside suite.ex", %{tmp_dir: dir} do
      uid = System.unique_integer([:positive])

      File.write!(Path.join(dir, "suite.ex"), """
      defmodule ResolverFixture#{uid}.HelperSuite do
        use ToastTest.Suite
      end
      """)

      File.write!(Path.join(dir, "helper.ex"), """
      defmodule ResolverFixture#{uid}.Helper do
        def value, do: 42
      end
      """)

      Resolver.compile_suite(dir)
      helper = :"Elixir.ResolverFixture#{uid}.Helper"
      assert Code.ensure_loaded?(helper)
      assert helper.value() == 42
    end

    @tag :tmp_dir
    test "raises when no suite.ex exists", %{tmp_dir: dir} do
      assert_raise ArgumentError, ~r/no suite\.ex found/, fn ->
        Resolver.compile_suite(dir)
      end
    end
  end

  describe "compile_test_files/2" do
    @tag :tmp_dir
    test "excludes specified file", %{tmp_dir: dir} do
      uid = System.unique_integer([:positive])

      file_a = Path.join(dir, "test_a.exs")
      file_b = Path.join(dir, "test_b.exs")

      File.write!(file_a, """
      defmodule ResolverFixture#{uid}.TestA do
        use ExUnit.Case, async: false
        test "a", do: assert(true)
      end
      """)

      File.write!(file_b, """
      defmodule ResolverFixture#{uid}.TestB do
        use ExUnit.Case, async: false
        test "b", do: assert(true)
      end
      """)

      modules = Resolver.compile_test_files(dir, exclude: file_a)
      module_names = Enum.map(modules, &to_string/1)
      assert Enum.any?(module_names, &(&1 =~ "TestB"))
      refute Enum.any?(module_names, &(&1 =~ "TestA"))
    end
  end

  describe "filter_modules_under/2" do
    test "includes modules whose source is under the directory" do
      modules = Resolver.filter_modules_under([Fixtures.SuiteTest], __DIR__)
      assert Fixtures.SuiteTest in modules
    end

    test "excludes modules from other directories" do
      modules = Resolver.filter_modules_under([Fixtures.SuiteTest], "/nonexistent/path")
      assert modules == []
    end
  end

  defp create_fixture(base_dir) do
    suite_dir = Path.join(base_dir, "resolver_fixture_#{System.unique_integer([:positive])}")
    File.mkdir_p!(suite_dir)
    uid = System.unique_integer([:positive])

    File.write!(Path.join(suite_dir, "suite.ex"), """
    defmodule ResolverFixture#{uid}.Suite do
      use ToastTest.Suite
    end
    """)

    File.write!(Path.join(suite_dir, "test_example.exs"), """
    defmodule ResolverFixture#{uid}.ExampleTest do
      use ExUnit.Case, async: false
      def __toast_suite__, do: ResolverFixture#{uid}.Suite
      test "it works", do: assert(true)
    end
    """)

    {suite_dir, uid}
  end
end
