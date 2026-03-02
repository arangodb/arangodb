defmodule Mix.Tasks.ToastTest do
  use ExUnit.Case, async: true

  alias Mix.Tasks.Toast.Helpers

  describe "parse_suite_args/2" do
    test "empty args returns :all with empty filters" do
      assert {:all, %{}} = Helpers.parse_suite_args([], "/suites")
    end

    test "single suite name returns list with that name" do
      {names, filters} = Helpers.parse_suite_args(["smoke"], "/suites")

      assert names == ["smoke"]
      assert filters == %{}
    end

    test "multiple suite names preserves order and deduplicates" do
      {names, _} = Helpers.parse_suite_args(["b", "a", "b"], "/suites")

      assert names == ["b", "a"]
    end

    test "suite/file syntax extracts file filter" do
      {names, filters} = Helpers.parse_suite_args(["smoke/test_basics.exs"], "/suites")

      assert names == ["smoke"]
      assert filters == %{"smoke" => ["test_basics.exs"]}
    end

    test "suite/file:line syntax passes through to filters" do
      {names, filters} = Helpers.parse_suite_args(["smoke/test_basics.exs:42"], "/suites")

      assert names == ["smoke"]
      assert filters == %{"smoke" => ["test_basics.exs:42"]}
    end

    test "multiple files for same suite accumulates filters" do
      args = ["smoke/test_a.exs", "smoke/test_b.exs"]
      {names, filters} = Helpers.parse_suite_args(args, "/suites")

      assert names == ["smoke"]
      assert "test_a.exs" in filters["smoke"]
      assert "test_b.exs" in filters["smoke"]
    end

    test "mixed bare suite names and file-filtered suites" do
      args = ["smoke/test_a.exs", "cluster"]
      {names, filters} = Helpers.parse_suite_args(args, "/suites")

      assert names == ["smoke", "cluster"]
      assert Map.has_key?(filters, "smoke")
      refute Map.has_key?(filters, "cluster")
    end

    test "suites_dir argument is unused in current implementation" do
      # The second argument is accepted but not used for parsing.
      {names, _} = Helpers.parse_suite_args(["foo"], "/anything")
      assert names == ["foo"]
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

    test "passes through trace" do
      opts = Helpers.process_opts(trace: true)

      assert opts[:trace] == true
    end

    test "passes through max_cases" do
      opts = Helpers.process_opts(max_cases: 4)

      assert opts[:max_cases] == 4
    end

    test "passes through timeout" do
      opts = Helpers.process_opts(timeout: 5000)

      assert opts[:timeout] == 5000
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

    test "color true sets colors: [enabled: true]" do
      opts = Helpers.process_opts(color: true)

      assert opts[:colors] == [enabled: true]
    end

    test "color false sets colors: [enabled: false]" do
      opts = Helpers.process_opts(color: false)

      assert opts[:colors] == [enabled: false]
    end

    test "no color option omits colors key" do
      opts = Helpers.process_opts([])

      refute Keyword.has_key?(opts, :colors)
    end

    test "formatter option converts string to module" do
      opts = Helpers.process_opts(formatter: "ExUnit.CLIFormatter")

      assert opts[:formatters] == [ExUnit.CLIFormatter]
    end

    test "strips non-ExUnit keys from output" do
      opts = Helpers.process_opts(build_dir: "/foo", cluster: true)

      refute Keyword.has_key?(opts, :build_dir)
      refute Keyword.has_key?(opts, :cluster)
    end
  end

  describe "opts_to_config_list/1" do
    test "empty opts produces empty config" do
      assert Helpers.opts_to_config_list([]) == []
    end

    test "maps build_dir to :build_dir" do
      config = Helpers.opts_to_config_list(build_dir: "/path")

      assert Keyword.get(config, :build_dir) == "/path"
    end

    test "maps sanitizer to :explicit_sanitizer" do
      config = Helpers.opts_to_config_list(sanitizer: "tsan")

      assert Keyword.get(config, :explicit_sanitizer) == "tsan"
    end

    test "maps replication_factor to :cluster_replication_factor" do
      config = Helpers.opts_to_config_list(replication_factor: 3)

      assert Keyword.get(config, :cluster_replication_factor) == 3
    end

    test "--cluster sets deployment_mode: :cluster" do
      config = Helpers.opts_to_config_list(cluster: true)

      assert Keyword.get(config, :deployment_mode) == :cluster
    end

    test "--single sets deployment_mode: :single_server" do
      config = Helpers.opts_to_config_list(single: true)

      assert Keyword.get(config, :deployment_mode) == :single_server
    end

    test "neither --cluster nor --single omits deployment_mode" do
      config = Helpers.opts_to_config_list(build_dir: "/path")

      refute Keyword.has_key?(config, :deployment_mode)
    end

    test "--cluster takes precedence over --single" do
      config = Helpers.opts_to_config_list(cluster: true, single: true)

      assert Keyword.get(config, :deployment_mode) == :cluster
    end

    test "maps ci to :ci" do
      config = Helpers.opts_to_config_list(ci: true)

      assert Keyword.get(config, :ci) == true
    end

    test "maps all timeout options" do
      config =
        Helpers.opts_to_config_list(
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
        Helpers.opts_to_config_list(
          cluster_agents: 5,
          cluster_dbservers: 2,
          cluster_coordinators: 3
        )

      assert Keyword.get(config, :cluster_agents) == 5
      assert Keyword.get(config, :cluster_dbservers) == 2
      assert Keyword.get(config, :cluster_coordinators) == 3
    end

    test "ignores keys not in the mapping" do
      config = Helpers.opts_to_config_list(trace: true)

      assert config == []
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

  describe "has_sanitizer_errors?/1" do
    test "returns false for empty suite list" do
      refute Helpers.has_sanitizer_errors?([])
    end

    test "returns false for suite with nil diagnostics" do
      suites = [%{diagnostics: nil}]

      refute Helpers.has_sanitizer_errors?(suites)
    end

    test "returns false for suite with empty diagnostics map" do
      suites = [%{diagnostics: %{}}]

      refute Helpers.has_sanitizer_errors?(suites)
    end

    test "returns false when diagnostics has no sanitizer_errors key" do
      suites = [%{diagnostics: %{"s1" => %{server: %{log_file: "/log"}}}}]

      refute Helpers.has_sanitizer_errors?(suites)
    end

    test "returns false when sanitizer_errors is empty list" do
      suites = [%{diagnostics: %{"s1" => %{sanitizer_errors: []}}}]

      refute Helpers.has_sanitizer_errors?(suites)
    end

    test "returns true for single-server sanitizer errors" do
      suites = [
        %{diagnostics: %{"s1" => %{sanitizer_errors: [%{file_path: "/tmp/san.log"}]}}}
      ]

      assert Helpers.has_sanitizer_errors?(suites)
    end

    test "returns true for cluster-level sanitizer errors in nested diagnostics" do
      suites = [
        %{
          diagnostics: %{
            "dbserver1" => %{sanitizer_errors: [%{file_path: "/tmp/san.log"}]},
            "dbserver2" => %{sanitizer_errors: []}
          }
        }
      ]

      assert Helpers.has_sanitizer_errors?(suites)
    end

    test "returns false when all cluster servers have empty sanitizer errors" do
      suites = [
        %{
          diagnostics: %{
            "dbserver1" => %{sanitizer_errors: []},
            "dbserver2" => %{sanitizer_errors: []}
          }
        }
      ]

      refute Helpers.has_sanitizer_errors?(suites)
    end

    test "returns true if any suite has errors among multiple suites" do
      suites = [
        %{diagnostics: %{"s1" => %{sanitizer_errors: []}}},
        %{diagnostics: %{"s2" => %{sanitizer_errors: [%{file_path: "/err"}]}}}
      ]

      assert Helpers.has_sanitizer_errors?(suites)
    end

    test "handles suite maps accessed via keyword-style :diagnostics" do
      suites = [[diagnostics: %{"s1" => %{sanitizer_errors: [%{file_path: "/err"}]}}]]

      assert Helpers.has_sanitizer_errors?(suites)
    end
  end

  describe "build_suite_diagnostics/1" do
    test "returns empty list for empty suites" do
      assert Helpers.build_suite_diagnostics([]) == []
    end

    test "extracts name from suite_module" do
      suites = [[suite_module: MyApp.Smoke, diagnostics: nil]]
      [diag] = Helpers.build_suite_diagnostics(suites)

      assert diag.name == "MyApp.Smoke"
    end

    test "extracts single-server log files" do
      suites = [
        [
          suite_module: MyApp.Test,
          diagnostics: %{"s1" => %{server: %{log_file: "/tmp/arangod.log"}}}
        ]
      ]

      [diag] = Helpers.build_suite_diagnostics(suites)
      assert diag.log_files == ["/tmp/arangod.log"]
    end

    test "extracts cluster log files from nested server diagnostics" do
      suites = [
        [
          suite_module: MyApp.Test,
          diagnostics: %{
            "db1" => %{server: %{log_file: "/tmp/db1.log"}},
            "db2" => %{server: %{log_file: "/tmp/db2.log"}}
          }
        ]
      ]

      [diag] = Helpers.build_suite_diagnostics(suites)
      assert Enum.sort(diag.log_files) == ["/tmp/db1.log", "/tmp/db2.log"]
    end

    test "extracts sanitizer file paths" do
      suites = [
        [
          suite_module: MyApp.Test,
          diagnostics: %{
            "s1" => %{
              sanitizer_errors: [
                %{file_path: "/tmp/tsan.log"},
                %{file_path: "/tmp/asan.log"}
              ]
            }
          }
        ]
      ]

      [diag] = Helpers.build_suite_diagnostics(suites)
      assert Enum.sort(diag.sanitizer_files) == ["/tmp/asan.log", "/tmp/tsan.log"]
    end

    test "extracts cluster sanitizer file paths" do
      suites = [
        [
          suite_module: MyApp.Test,
          diagnostics: %{
            "srv1" => %{sanitizer_errors: [%{file_path: "/tmp/s1.log"}]},
            "srv2" => %{sanitizer_errors: [%{file_path: "/tmp/s2.log"}]}
          }
        ]
      ]

      [diag] = Helpers.build_suite_diagnostics(suites)
      assert Enum.sort(diag.sanitizer_files) == ["/tmp/s1.log", "/tmp/s2.log"]
    end

    test "extracts core dump paths" do
      suites = [
        [
          suite_module: MyApp.Test,
          diagnostics: %{
            "s1" => %{coredump_reports: [%{core_path: "/cores/core.1234"}]}
          }
        ]
      ]

      [diag] = Helpers.build_suite_diagnostics(suites)
      assert diag.core_dumps == ["/cores/core.1234"]
    end

    test "extracts cluster core dump paths" do
      suites = [
        [
          suite_module: MyApp.Test,
          diagnostics: %{
            "srv1" => %{coredump_reports: [%{core_path: "/cores/core.1"}]},
            "srv2" => %{coredump_reports: [%{core_path: "/cores/core.2"}]}
          }
        ]
      ]

      [diag] = Helpers.build_suite_diagnostics(suites)
      assert Enum.sort(diag.core_dumps) == ["/cores/core.1", "/cores/core.2"]
    end

    test "nil diagnostics produces empty lists for all file fields" do
      suites = [[suite_module: MyApp.Test, diagnostics: nil]]
      [diag] = Helpers.build_suite_diagnostics(suites)

      assert diag.log_files == []
      assert diag.sanitizer_files == []
      assert diag.core_dumps == []
      assert diag.crash_reports == []
      assert diag.agency_dumps == []
    end

    test "filters out nil file_path values from sanitizer errors" do
      suites = [
        [
          suite_module: MyApp.Test,
          diagnostics: %{
            "s1" => %{
              sanitizer_errors: [
                %{file_path: "/tmp/ok.log"},
                %{file_path: nil}
              ]
            }
          }
        ]
      ]

      [diag] = Helpers.build_suite_diagnostics(suites)
      assert diag.sanitizer_files == ["/tmp/ok.log"]
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
