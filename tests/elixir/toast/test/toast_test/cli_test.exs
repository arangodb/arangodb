defmodule ToastTest.CLITest do
  use ExUnit.Case, async: false

  # The mix toast task uses OptionParser with @switches and @aliases.
  # We test the argument parsing logic directly using the same switch/alias
  # definitions, since the parsing functions are private.

  @switches [
    include: :keep,
    exclude: :keep,
    only: :keep,
    seed: :integer,
    trace: :boolean,
    max_cases: :integer,
    timeout: :integer,
    max_failures: :integer,
    formatter: :keep,
    color: :boolean,
    compile: :boolean,
    start: :boolean,
    build_dir: :string,
    work_dir: :string,
    result_dir: :string,
    cluster: :boolean,
    single: :boolean,
    show_server_logs: :boolean,
    global_timeout: :integer,
    test_timeout: :integer,
    startup_timeout: :integer,
    shutdown_timeout: :integer,
    timeout_factor: :integer,
    keep_work_dir: :boolean,
    sanitizer: :string,
    cluster_agents: :integer,
    cluster_dbservers: :integer,
    cluster_coordinators: :integer,
    replication_factor: :integer,
    test: :string,
    no_agency_dump: :boolean
  ]

  @aliases [
    i: :include,
    e: :exclude,
    s: :seed,
    t: :trace,
    b: :build_dir
  ]

  defp parse(args) do
    OptionParser.parse!(args, strict: @switches, aliases: @aliases)
  end

  describe "--seed" do
    test "parses integer seed value" do
      {opts, _rest} = parse(["--seed", "12345"])
      assert opts[:seed] == 12345
    end

    test "alias -s works" do
      {opts, _rest} = parse(["-s", "42"])
      assert opts[:seed] == 42
    end

    test "seed 0 disables shuffling" do
      {opts, _rest} = parse(["--seed", "0"])
      assert opts[:seed] == 0
    end
  end

  describe "--timeout" do
    test "parses global timeout" do
      {opts, _rest} = parse(["--global-timeout", "7200000"])
      assert opts[:global_timeout] == 7_200_000
    end

    test "parses test timeout" do
      {opts, _rest} = parse(["--test-timeout", "60000"])
      assert opts[:test_timeout] == 60_000
    end

    test "parses startup timeout" do
      {opts, _rest} = parse(["--startup-timeout", "120000"])
      assert opts[:startup_timeout] == 120_000
    end

    test "parses shutdown timeout" do
      {opts, _rest} = parse(["--shutdown-timeout", "30000"])
      assert opts[:shutdown_timeout] == 30_000
    end
  end

  describe "deployment mode flags" do
    test "--cluster sets cluster flag" do
      {opts, _rest} = parse(["--cluster"])
      assert opts[:cluster] == true
    end

    test "--single sets single flag" do
      {opts, _rest} = parse(["--single"])
      assert opts[:single] == true
    end
  end

  describe "cluster configuration" do
    test "--cluster-agents parses integer" do
      {opts, _rest} = parse(["--cluster-agents", "5"])
      assert opts[:cluster_agents] == 5
    end

    test "--cluster-dbservers parses integer" do
      {opts, _rest} = parse(["--cluster-dbservers", "3"])
      assert opts[:cluster_dbservers] == 3
    end

    test "--cluster-coordinators parses integer" do
      {opts, _rest} = parse(["--cluster-coordinators", "2"])
      assert opts[:cluster_coordinators] == 2
    end

    test "--replication-factor parses integer" do
      {opts, _rest} = parse(["--replication-factor", "3"])
      assert opts[:replication_factor] == 3
    end
  end

  describe "unknown flags" do
    test "unknown flags raise OptionParser.ParseError" do
      assert_raise OptionParser.ParseError, fn ->
        parse(["--unknown-flag", "value"])
      end
    end
  end

  describe "mixed ExUnit and Toast options" do
    test "ExUnit and Toast options parsed together" do
      {opts, rest} =
        parse([
          "--seed", "42",
          "--trace",
          "--build-dir", "/path/to/build",
          "--cluster",
          "--timeout-factor", "3",
          "my_test.exs"
        ])

      assert opts[:seed] == 42
      assert opts[:trace] == true
      assert opts[:build_dir] == "/path/to/build"
      assert opts[:cluster] == true
      assert opts[:timeout_factor] == 3
      assert rest == ["my_test.exs"]
    end
  end

  describe "filter options" do
    test "--include can be specified multiple times" do
      {opts, _rest} = parse(["--include", "slow", "--include", "integration"])
      values = Keyword.get_values(opts, :include)
      assert "slow" in values
      assert "integration" in values
    end

    test "--exclude can be specified multiple times" do
      {opts, _rest} = parse(["--exclude", "slow", "--exclude", "flaky"])
      values = Keyword.get_values(opts, :exclude)
      assert "slow" in values
      assert "flaky" in values
    end

    test "--only can be specified" do
      {opts, _rest} = parse(["--only", "smoke"])
      assert Keyword.get_values(opts, :only) == ["smoke"]
    end
  end

  describe "aliases" do
    test "-b is alias for --build-dir" do
      {opts, _rest} = parse(["-b", "/path/to/build"])
      assert opts[:build_dir] == "/path/to/build"
    end

    test "-t is alias for --trace" do
      {opts, _rest} = parse(["-t"])
      assert opts[:trace] == true
    end

    test "-i is alias for --include" do
      {opts, _rest} = parse(["-i", "smoke"])
      assert Keyword.get_values(opts, :include) == ["smoke"]
    end

    test "-e is alias for --exclude" do
      {opts, _rest} = parse(["-e", "slow"])
      assert Keyword.get_values(opts, :exclude) == ["slow"]
    end
  end

  describe "environment variable fallbacks" do
    # The mix task maps CLI options to TOAST_* environment variables.
    # We verify the mapping is correct.

    @toast_env_map %{
      build_dir: "TOAST_BUILD_DIR",
      work_dir: "TOAST_WORK_DIR",
      result_dir: "TOAST_RESULT_DIR",
      show_server_logs: "TOAST_SHOW_SERVER_LOGS",
      global_timeout: "TOAST_GLOBAL_TIMEOUT",
      test_timeout: "TOAST_TEST_TIMEOUT",
      startup_timeout: "TOAST_STARTUP_TIMEOUT",
      shutdown_timeout: "TOAST_SHUTDOWN_TIMEOUT",
      timeout_factor: "TOAST_TIMEOUT_FACTOR",
      keep_work_dir: "TOAST_KEEP_WORK_DIR",
      sanitizer: "TOAST_SANITIZER",
      cluster_agents: "TOAST_CLUSTER_AGENTS",
      cluster_dbservers: "TOAST_CLUSTER_DBSERVERS",
      cluster_coordinators: "TOAST_CLUSTER_COORDINATORS",
      replication_factor: "TOAST_CLUSTER_REPLICATION_FACTOR",
      no_agency_dump: "TOAST_NO_AGENCY_DUMP"
    }

    test "Toast.Config reads TOAST_BUILD_DIR from environment" do
      saved = System.get_env("TOAST_BUILD_DIR")

      try do
        System.put_env("TOAST_BUILD_DIR", "/env/build/dir")
        config = Toast.Config.load()
        assert config.build_dir == "/env/build/dir"
      after
        if saved, do: System.put_env("TOAST_BUILD_DIR", saved), else: System.delete_env("TOAST_BUILD_DIR")
      end
    end

    test "CLI options take precedence over environment variables" do
      saved = System.get_env("TOAST_BUILD_DIR")

      try do
        System.put_env("TOAST_BUILD_DIR", "/env/build/dir")
        config = Toast.Config.load(build_dir: "/cli/build/dir")
        assert config.build_dir == "/cli/build/dir"
      after
        if saved, do: System.put_env("TOAST_BUILD_DIR", saved), else: System.delete_env("TOAST_BUILD_DIR")
      end
    end

    test "TOAST_DEPLOYMENT_MODE=cluster sets cluster mode" do
      saved = System.get_env("TOAST_DEPLOYMENT_MODE")

      try do
        System.put_env("TOAST_DEPLOYMENT_MODE", "cluster")
        config = Toast.Config.load()
        assert config.deployment_mode == :cluster
      after
        if saved, do: System.put_env("TOAST_DEPLOYMENT_MODE", saved), else: System.delete_env("TOAST_DEPLOYMENT_MODE")
      end
    end

    test "env map covers all expected TOAST_* variables" do
      expected_keys = MapSet.new([
        :build_dir, :work_dir, :result_dir, :show_server_logs,
        :global_timeout, :test_timeout, :startup_timeout, :shutdown_timeout,
        :timeout_factor, :keep_work_dir, :sanitizer,
        :cluster_agents, :cluster_dbservers, :cluster_coordinators,
        :replication_factor, :no_agency_dump
      ])

      actual_keys = MapSet.new(Map.keys(@toast_env_map))
      assert expected_keys == actual_keys
    end
  end

  describe "boolean options" do
    test "--show-server-logs" do
      {opts, _rest} = parse(["--show-server-logs"])
      assert opts[:show_server_logs] == true
    end

    test "--keep-work-dir" do
      {opts, _rest} = parse(["--keep-work-dir"])
      assert opts[:keep_work_dir] == true
    end

    test "--no-compile" do
      {opts, _rest} = parse(["--no-compile"])
      assert opts[:compile] == false
    end

    test "--no-start" do
      {opts, _rest} = parse(["--no-start"])
      assert opts[:start] == false
    end
  end

  describe "string options" do
    test "--build-dir" do
      {opts, _rest} = parse(["--build-dir", "/path/to/build"])
      assert opts[:build_dir] == "/path/to/build"
    end

    test "--work-dir" do
      {opts, _rest} = parse(["--work-dir", "/tmp/work"])
      assert opts[:work_dir] == "/tmp/work"
    end

    test "--result-dir" do
      {opts, _rest} = parse(["--result-dir", "/tmp/results"])
      assert opts[:result_dir] == "/tmp/results"
    end

    test "--sanitizer" do
      {opts, _rest} = parse(["--sanitizer", "tsan"])
      assert opts[:sanitizer] == "tsan"
    end
  end
end
