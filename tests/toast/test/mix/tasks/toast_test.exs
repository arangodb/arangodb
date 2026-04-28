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

defmodule Mix.Tasks.ToastTest do
  use ExUnit.Case, async: true

  alias Mix.Tasks.Toast.Helpers

  describe "parse_suite_args/1" do
    test "empty args returns :all with empty filters" do
      assert {:all, %{}} = Helpers.parse_suite_args([])
    end

    test "single suite name returns list with that name" do
      {names, filters} = Helpers.parse_suite_args(["smoke"])

      assert names == ["smoke"]
      assert filters == %{}
    end

    test "multiple suite names preserves order and deduplicates" do
      {names, _} = Helpers.parse_suite_args(["b", "a", "b"])

      assert names == ["b", "a"]
    end

    test "suite/file syntax extracts file filter" do
      {names, filters} = Helpers.parse_suite_args(["smoke/test_basics.exs"])

      assert names == ["smoke"]
      assert filters == %{"smoke" => ["test_basics.exs"]}
    end

    test "suite/file:line syntax passes through to filters" do
      {names, filters} = Helpers.parse_suite_args(["smoke/test_basics.exs:42"])

      assert names == ["smoke"]
      assert filters == %{"smoke" => ["test_basics.exs:42"]}
    end

    test "multiple files for same suite accumulates filters" do
      args = ["smoke/test_a.exs", "smoke/test_b.exs"]
      {names, filters} = Helpers.parse_suite_args(args)

      assert names == ["smoke"]
      assert "test_a.exs" in filters["smoke"]
      assert "test_b.exs" in filters["smoke"]
    end

    test "mixed bare suite names and file-filtered suites" do
      args = ["smoke/test_a.exs", "cluster"]
      {names, filters} = Helpers.parse_suite_args(args)

      assert names == ["smoke", "cluster"]
      assert Map.has_key?(filters, "smoke")
      refute Map.has_key?(filters, "cluster")
    end
  end

  describe "normalize_arg/1" do
    test "passes through plain suite name" do
      assert Helpers.normalize_arg("smoke") == "smoke"
    end

    test "passes through suite/file format" do
      assert Helpers.normalize_arg("smoke/test_version.exs") == "smoke/test_version.exs"
    end

    test "strips suites/ prefix from suite name" do
      assert Helpers.normalize_arg("suites/smoke") == "smoke"
    end

    test "strips suites/ prefix from suite/file path" do
      assert Helpers.normalize_arg("suites/smoke/test_version.exs") == "smoke/test_version.exs"
    end

    test "strips suites/ prefix from suite/file:line path" do
      assert Helpers.normalize_arg("suites/smoke/test_version.exs:42") ==
               "smoke/test_version.exs:42"
    end

    test "does not strip suites prefix without slash" do
      assert Helpers.normalize_arg("suites_extra") == "suites_extra"
    end
  end

  describe "expand_args/2" do
    @tag :tmp_dir
    test "passes through non-glob args with normalization", %{tmp_dir: dir} do
      assert Helpers.expand_args(["smoke", "suites/cluster"], dir) ==
               ["smoke", "cluster"]
    end

    @tag :tmp_dir
    test "expands glob matching single file", %{tmp_dir: dir} do
      suite_dir = Path.join(dir, "smoke")
      File.mkdir_p!(suite_dir)
      File.write!(Path.join(suite_dir, "test_version.exs"), "")

      result = Helpers.expand_args(["smoke/test_v*.exs"], dir)

      assert result == ["smoke/test_version.exs"]
    end

    @tag :tmp_dir
    test "expands glob matching multiple files", %{tmp_dir: dir} do
      suite_dir = Path.join(dir, "smoke")
      File.mkdir_p!(suite_dir)
      File.write!(Path.join(suite_dir, "test_aql.exs"), "")
      File.write!(Path.join(suite_dir, "test_version.exs"), "")
      File.write!(Path.join(suite_dir, "test_collection.exs"), "")

      result = Helpers.expand_args(["smoke/test_*.exs"], dir) |> Enum.sort()

      assert result == [
               "smoke/test_aql.exs",
               "smoke/test_collection.exs",
               "smoke/test_version.exs"
             ]
    end

    @tag :tmp_dir
    test "expands glob with suites/ prefix", %{tmp_dir: dir} do
      suite_dir = Path.join(dir, "smoke")
      File.mkdir_p!(suite_dir)
      File.write!(Path.join(suite_dir, "test_version.exs"), "")

      result = Helpers.expand_args(["suites/smoke/test_v*.exs"], dir)

      assert result == ["smoke/test_version.exs"]
    end

    @tag :tmp_dir
    test "expands glob across suites with wildcard suite name", %{tmp_dir: dir} do
      for suite <- ["smoke", "cluster"] do
        suite_dir = Path.join(dir, suite)
        File.mkdir_p!(suite_dir)
        File.write!(Path.join(suite_dir, "test_version.exs"), "")
      end

      result = Helpers.expand_args(["*/test_version.exs"], dir) |> Enum.sort()

      assert result == ["cluster/test_version.exs", "smoke/test_version.exs"]
    end

    @tag :tmp_dir
    test "glob with no matches passes through normalized arg", %{tmp_dir: dir} do
      result = Helpers.expand_args(["smoke/test_nonexistent*.exs"], dir)

      assert result == ["smoke/test_nonexistent*.exs"]
    end

    @tag :tmp_dir
    test "mixes glob and non-glob args", %{tmp_dir: dir} do
      suite_dir = Path.join(dir, "smoke")
      File.mkdir_p!(suite_dir)
      File.write!(Path.join(suite_dir, "test_version.exs"), "")

      result = Helpers.expand_args(["cluster", "smoke/test_v*.exs"], dir)

      assert result == ["cluster", "smoke/test_version.exs"]
    end
  end

  describe "parse_file_specs/1" do
    test "plain filename" do
      {files, lines} = Helpers.parse_file_specs(["test_foo.exs"])

      assert "test_foo.exs" in files
      assert lines == []
    end

    test "file:line syntax extracts line number" do
      {files, lines} = Helpers.parse_file_specs(["test_foo.exs:42"])

      assert "test_foo.exs" in files
      assert {"test_foo.exs", 42} in lines
    end

    test "multiple specs accumulate" do
      {files, lines} = Helpers.parse_file_specs(["test_a.exs:10", "test_b.exs"])

      assert "test_a.exs" in files
      assert "test_b.exs" in files
      assert {"test_a.exs", 10} in lines
      assert length(lines) == 1
    end

    test "invalid line number is treated as plain filename" do
      {files, lines} = Helpers.parse_file_specs(["test_foo.exs:abc"])

      assert "test_foo.exs" in files
      assert lines == []
    end

    test "partial integer parse is treated as plain filename" do
      {files, lines} = Helpers.parse_file_specs(["test_foo.exs:42abc"])

      assert "test_foo.exs" in files
      assert lines == []
    end

    test "empty list" do
      assert {[], []} = Helpers.parse_file_specs([])
    end
  end

  describe "process_opts/1" do
    test "returns autorun: false in output" do
      opts = Helpers.process_opts([])

      assert opts[:autorun] == false
    end

    test "sets default exit_status to 2" do
      opts = Helpers.process_opts([])

      assert opts[:exit_status] == 2
    end

    test "passes through max_failures" do
      opts = Helpers.process_opts(max_failures: 3)

      assert opts[:max_failures] == 3
    end

    test "parses include filters via ExUnit.Filters" do
      opts = Helpers.process_opts(include: "slow")

      assert opts[:include] == ExUnit.Filters.parse(["slow"])
    end

    test "parses exclude filters via ExUnit.Filters" do
      opts = Helpers.process_opts(exclude: "integration")

      assert opts[:exclude] == ExUnit.Filters.parse(["integration"])
    end

    test "only adds to include and adds :test to exclude" do
      opts = Helpers.process_opts(only: "smoke")

      assert opts[:include] == ExUnit.Filters.parse(["smoke"])
      assert :test in opts[:exclude]
    end

    test "only merges with existing include" do
      opts = Helpers.process_opts(only: "smoke", include: "fast")

      include = opts[:include]
      assert :smoke in include or {:smoke, true} in include
      assert :fast in include or {:fast, true} in include
    end

    test "strips non-ExUnit keys from output" do
      opts = Helpers.process_opts(build_dir: "/foo", cluster: true)

      refute Keyword.has_key?(opts, :build_dir)
      refute Keyword.has_key?(opts, :cluster)
    end
  end

  describe "opts_to_env_list/1" do
    test "empty opts produces empty config" do
      assert Helpers.opts_to_env_list([]) == []
    end

    test "maps build_dir to :build_dir" do
      config = Helpers.opts_to_env_list(build_dir: "/path")

      assert Keyword.get(config, :build_dir) == "/path"
    end

    test "maps sanitizer to :sanitizer_override" do
      config = Helpers.opts_to_env_list(sanitizer: "tsan")

      assert Keyword.get(config, :sanitizer_override) == "tsan"
    end

    test "maps replication_factor to :cluster_replication_factor" do
      config = Helpers.opts_to_env_list(replication_factor: 3)

      assert Keyword.get(config, :cluster_replication_factor) == 3
    end

    test "--cluster sets deployment_mode: :cluster" do
      config = Helpers.opts_to_env_list(cluster: true)

      assert Keyword.get(config, :deployment_mode) == :cluster
    end

    test "--single sets deployment_mode: :single_server" do
      config = Helpers.opts_to_env_list(single: true)

      assert Keyword.get(config, :deployment_mode) == :single_server
    end

    test "neither --cluster nor --single omits deployment_mode" do
      config = Helpers.opts_to_env_list(build_dir: "/path")

      refute Keyword.has_key?(config, :deployment_mode)
    end

    test "--cluster takes precedence over --single" do
      config = Helpers.opts_to_env_list(cluster: true, single: true)

      assert Keyword.get(config, :deployment_mode) == :cluster
    end

    test "maps ci to :ci" do
      config = Helpers.opts_to_env_list(ci: true)

      assert Keyword.get(config, :ci) == true
    end

    test "maps all timeout options" do
      config =
        Helpers.opts_to_env_list(
          global_timeout: 100,
          test_timeout: 200,
          startup_timeout: 300,
          shutdown_timeout: 400,
          timeout_factor: 5
        )

      assert Keyword.get(config, :global_timeout) == 100
      assert Keyword.get(config, :test_timeout) == 200
      assert Keyword.get(config, :startup_timeout) == 300
      assert Keyword.get(config, :shutdown_timeout) == 400
      assert Keyword.get(config, :timeout_factor) == 5
    end

    test "maps cluster topology options" do
      config =
        Helpers.opts_to_env_list(
          cluster_agents: 5,
          cluster_dbservers: 2,
          cluster_coordinators: 3
        )

      assert Keyword.get(config, :cluster_agents) == 5
      assert Keyword.get(config, :cluster_dbservers) == 2
      assert Keyword.get(config, :cluster_coordinators) == 3
    end

    test "ignores keys not in the mapping" do
      config = Helpers.opts_to_env_list(trace: true)

      assert config == []
    end

    test "--http2 sets protocol: :http2" do
      config = Helpers.opts_to_env_list(http2: true)

      assert Keyword.get(config, :protocol) == :http2
    end

    test "--no-http2 sets protocol: :http1" do
      config = Helpers.opts_to_env_list(http2: false)

      assert Keyword.get(config, :protocol) == :http1
    end

    test "no --http2 omits protocol" do
      config = Helpers.opts_to_env_list([])

      refute Keyword.has_key?(config, :protocol)
    end
  end

  describe "discover_suite_files/1" do
    @tag :tmp_dir
    test "classifies .ex helpers, test_*.exs tests, and excludes suite.ex", %{tmp_dir: dir} do
      File.write!(Path.join(dir, "suite.ex"), "")
      File.write!(Path.join(dir, "my_helper.ex"), "")
      File.write!(Path.join(dir, "test_basics.exs"), "")
      File.write!(Path.join(dir, "test_advanced.exs"), "")
      File.write!(Path.join(dir, "not_a_test.exs"), "")

      {helpers, test_files} = Helpers.discover_suite_files(dir)

      assert length(helpers) == 1
      assert Path.basename(hd(helpers)) == "my_helper.ex"

      test_basenames = Enum.map(test_files, &Path.basename/1) |> Enum.sort()
      assert test_basenames == ["test_advanced.exs", "test_basics.exs"]
    end

    @tag :tmp_dir
    test "returns empty lists for empty directory", %{tmp_dir: dir} do
      assert {[], []} = Helpers.discover_suite_files(dir)
    end

    @tag :tmp_dir
    test "excludes suite.ex from helpers", %{tmp_dir: dir} do
      File.write!(Path.join(dir, "suite.ex"), "")

      {helpers, _} = Helpers.discover_suite_files(dir)
      assert helpers == []
    end

    @tag :tmp_dir
    test ".exs files not starting with test_ are excluded from test files", %{tmp_dir: dir} do
      File.write!(Path.join(dir, "helper.exs"), "")
      File.write!(Path.join(dir, "setup.exs"), "")

      {_, test_files} = Helpers.discover_suite_files(dir)
      assert test_files == []
    end

    @tag :tmp_dir
    test "does not recurse into subdirectories", %{tmp_dir: dir} do
      subdir = Path.join(dir, "sub")
      File.mkdir_p!(subdir)
      File.write!(Path.join(subdir, "test_nested.exs"), "")

      {_, test_files} = Helpers.discover_suite_files(dir)
      assert test_files == []
    end
  end

  describe "apply_file_filters/3" do
    test "empty filters returns all files unchanged with empty line_filters" do
      files = ["/suites/smoke/test_a.exs", "/suites/smoke/test_b.exs"]

      assert {^files, []} = Helpers.apply_file_filters(files, %{}, "/suites/smoke")
    end

    test "filters matching suite files by basename" do
      files = ["/suites/smoke/test_a.exs", "/suites/smoke/test_b.exs"]
      filters = %{"smoke" => ["test_a.exs"]}

      {filtered, _} = Helpers.apply_file_filters(files, filters, "/suites/smoke")

      assert length(filtered) == 1
      assert Path.basename(hd(filtered)) == "test_a.exs"
    end

    test "returns line filters from file:line specs" do
      files = ["/suites/smoke/test_a.exs"]
      filters = %{"smoke" => ["test_a.exs:42"]}

      {filtered, line_filters} = Helpers.apply_file_filters(files, filters, "/suites/smoke")

      assert length(filtered) == 1
      assert {"test_a.exs", 42} in line_filters
    end

    test "filters for different suite name returns all files" do
      files = ["/suites/smoke/test_a.exs"]
      filters = %{"cluster" => ["test_x.exs"]}

      {result, line_filters} = Helpers.apply_file_filters(files, filters, "/suites/smoke")

      assert result == files
      assert line_filters == []
    end

    test "non-matching file filter returns empty list" do
      files = ["/suites/smoke/test_a.exs"]
      filters = %{"smoke" => ["test_nonexistent.exs"]}

      {filtered, _} = Helpers.apply_file_filters(files, filters, "/suites/smoke")

      assert filtered == []
    end
  end

  describe "build_suite_opts/3" do
    test "no pattern and no line filters returns empty opts" do
      assert Helpers.build_suite_opts([], [], nil) == []
    end

    test "test name pattern is set when provided" do
      opts = Helpers.build_suite_opts([], [], "my_test")

      assert opts[:test_name_pattern] == "my_test"
    end

    test "nil pattern omits test_name_pattern key" do
      opts = Helpers.build_suite_opts([], [], nil)

      refute Keyword.has_key?(opts, :test_name_pattern)
    end
  end
end
