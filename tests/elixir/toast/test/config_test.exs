defmodule Toast.ConfigTest do
  use ExUnit.Case, async: false

  alias Toast.Config

  @env_vars ~w(
    TOAST_BUILD_DIR
    TOAST_BASE_DIR
    TOAST_RESULT_DIR
    TOAST_COREDUMP_DIR
    TOAST_DEPLOYMENT_MODE
    TOAST_SHOW_SERVER_LOGS
    TOAST_GLOBAL_TIMEOUT
    TOAST_TEST_TIMEOUT
    TOAST_STARTUP_TIMEOUT
    TOAST_SHUTDOWN_TIMEOUT
    TOAST_TIMEOUT_FACTOR
    TOAST_CLUSTER_AGENTS
    TOAST_CLUSTER_DBSERVERS
    TOAST_CLUSTER_COORDINATORS
    TOAST_CLUSTER_REPLICATION_FACTOR
    TOAST_API_VERSION
    TOAST_DEBUGGER
    TOAST_DUMP_AGENCY
    TOAST_COREDUMP_TIMEOUT
    TOAST_KEEP_DATA
    TOAST_SANITIZER
    TOAST_CI
  )

  @empty_config_dir Path.join(
                      System.tmp_dir!(),
                      "toast_config_test_empty_#{System.unique_integer([:positive])}"
                    )

  setup do
    saved = Map.new(@env_vars, fn var -> {var, System.get_env(var)} end)
    File.mkdir_p!(@empty_config_dir)

    on_exit(fn ->
      for {var, val} <- saved do
        if val, do: System.put_env(var, val), else: System.delete_env(var)
      end

      File.rm_rf!(@empty_config_dir)
    end)

    clear_env_vars()
    :ok
  end

  defp clear_env_vars do
    Enum.each(@env_vars, &System.delete_env/1)
  end

  # Wraps Config.load to isolate from the real .toast.local.exs.
  # Tests in the ".toast.local.exs" describe block use their own temp dir.
  defp load(opts \\ []) do
    Config.load(Keyword.put_new(opts, :local_config_dir, @empty_config_dir))
  end

  describe "defaults" do
    test "returns struct with expected defaults" do
      config = load()

      assert %Config{} = config
      assert config.build_dir == nil
      assert config.deployment_mode == :single_server
      assert config.show_server_logs == false
      assert config.server_args == %{}
      assert config.global_timeout == 3_600_000
      assert config.test_timeout == 300_000
      assert config.startup_timeout == 60_000
      assert config.shutdown_timeout == 60_000
      assert config.timeout_factor == 1
      assert config.api_version == nil
      assert config.debugger == :auto
      assert config.dump_agency_on_error == true
      assert config.coredump_timeout == 180_000
    end

    test "base_dir has unique default under tmp_dir" do
      config = load()

      assert String.starts_with?(config.base_dir, System.tmp_dir!())
      assert config.base_dir =~ ~r/toast\/\d{4}-\d{2}-\d{2}T\d{2}-\d{2}-\d{2}Z_[0-9A-F]{4}$/
    end
  end

  describe "TOAST_BUILD_DIR env var" do
    test "reads build_dir from environment" do
      System.put_env("TOAST_BUILD_DIR", "/custom/build")

      assert load().build_dir == "/custom/build"
    end
  end

  describe "TOAST_BASE_DIR env var" do
    test "reads base_dir from environment" do
      System.put_env("TOAST_BASE_DIR", "/custom/work")

      assert load().base_dir == "/custom/work"
    end
  end

  describe "TOAST_DEPLOYMENT_MODE env var" do
    test "cluster string maps to :cluster atom" do
      System.put_env("TOAST_DEPLOYMENT_MODE", "cluster")

      assert load().deployment_mode == :cluster
    end

    test "invalid values raise ArgumentError" do
      System.put_env("TOAST_DEPLOYMENT_MODE", "something_else")

      assert_raise ArgumentError, ~r/Invalid TOAST_DEPLOYMENT_MODE/, fn -> load() end
    end

    test "explicit single_server value" do
      System.put_env("TOAST_DEPLOYMENT_MODE", "single_server")

      assert load().deployment_mode == :single_server
    end

    test "nil defaults to :single_server" do
      assert load().deployment_mode == :single_server
    end
  end

  describe "TOAST_SHOW_SERVER_LOGS env var" do
    test "true string maps to true" do
      System.put_env("TOAST_SHOW_SERVER_LOGS", "true")

      assert load().show_server_logs == true
    end

    test "other values default to false" do
      System.put_env("TOAST_SHOW_SERVER_LOGS", "false")

      assert load().show_server_logs == false
    end

    test "nil defaults to false" do
      assert load().show_server_logs == false
    end
  end

  describe "TOAST_GLOBAL_TIMEOUT env var" do
    test "parses integer from string" do
      System.put_env("TOAST_GLOBAL_TIMEOUT", "7200000")

      assert load().global_timeout == 7_200_000
    end
  end

  describe "TOAST_TEST_TIMEOUT env var" do
    test "parses integer from string" do
      System.put_env("TOAST_TEST_TIMEOUT", "600000")

      assert load().test_timeout == 600_000
    end
  end

  describe "TOAST_STARTUP_TIMEOUT env var" do
    test "parses integer from string" do
      System.put_env("TOAST_STARTUP_TIMEOUT", "120000")

      assert load().startup_timeout == 120_000
    end
  end

  describe "keyword overrides" do
    test "take precedence over env vars" do
      System.put_env("TOAST_BUILD_DIR", "/env/build")
      System.put_env("TOAST_BASE_DIR", "/env/work")
      System.put_env("TOAST_DEPLOYMENT_MODE", "cluster")
      System.put_env("TOAST_SHOW_SERVER_LOGS", "true")
      System.put_env("TOAST_STARTUP_TIMEOUT", "120000")
      System.put_env("TOAST_GLOBAL_TIMEOUT", "7200000")
      System.put_env("TOAST_TEST_TIMEOUT", "600000")

      config =
        load(
          build_dir: "/opt/build",
          base_dir: "/opt/work",
          deployment_mode: :single_server,
          show_server_logs: false,
          startup_timeout: 5_000,
          global_timeout: 1_800_000,
          test_timeout: 120_000
        )

      assert config.build_dir == "/opt/build"
      assert config.base_dir == "/opt/work"
      assert config.deployment_mode == :single_server
      assert config.show_server_logs == false
      assert config.startup_timeout == 5_000
      assert config.global_timeout == 1_800_000
      assert config.test_timeout == 120_000
    end
  end

  describe "pos_int validation" do
    test "zero raises ArgumentError" do
      System.put_env("TOAST_STARTUP_TIMEOUT", "0")

      assert_raise ArgumentError, ~r/must be a positive integer/, fn ->
        load()
      end
    end

    test "negative value raises ArgumentError" do
      System.put_env("TOAST_CLUSTER_AGENTS", "-1")

      assert_raise ArgumentError, ~r/must be a positive integer/, fn ->
        load()
      end
    end

    test "non-numeric value raises ArgumentError" do
      System.put_env("TOAST_STARTUP_TIMEOUT", "abc")

      assert_raise ArgumentError, fn ->
        load()
      end
    end
  end

  describe "server_args" do
    test "passes through from opts" do
      args = %{"log.level" => "debug"}
      config = load(server_args: args)

      assert config.server_args == %{"log.level" => "debug"}
    end

    test "role-specific args default to empty maps" do
      config = load()

      assert config.coordinator_args == %{}
      assert config.dbserver_args == %{}
      assert config.agent_args == %{}
    end

    test "role-specific args pass through from opts" do
      config =
        load(
          coordinator_args: %{"query.memory-limit" => "1073741824"},
          dbserver_args: %{"rocksdb.block-cache-size" => "536870912"},
          agent_args: %{"agency.compaction-step-size" => "1000"}
        )

      assert config.coordinator_args == %{"query.memory-limit" => "1073741824"}
      assert config.dbserver_args == %{"rocksdb.block-cache-size" => "536870912"}
      assert config.agent_args == %{"agency.compaction-step-size" => "1000"}
    end
  end

  describe "timeout_factor" do
    test "defaults to 1 without sanitizer" do
      config = load()

      assert config.timeout_factor == 1
    end

    test "auto-detects factor 3 when sanitizer is present" do
      config = load(active_sanitizers: MapSet.new(["tsan"]))

      assert config.timeout_factor == 3
      assert config.test_timeout == 300_000 * 3
      assert config.global_timeout == 3_600_000 * 3
      assert config.startup_timeout == 60_000 * 3
      assert config.shutdown_timeout == 60_000 * 3
    end

    test "TOAST_TIMEOUT_FACTOR env var overrides auto-detection" do
      System.put_env("TOAST_TIMEOUT_FACTOR", "2")

      config = load(active_sanitizers: MapSet.new(["tsan"]))

      assert config.timeout_factor == 2
      assert config.test_timeout == 300_000 * 2
    end

    test "keyword override takes precedence" do
      config = load(timeout_factor: 5, active_sanitizers: MapSet.new(["tsan"]))

      assert config.timeout_factor == 5
      assert config.test_timeout == 300_000 * 5
    end

    test "factor multiplies explicitly set timeouts" do
      System.put_env("TOAST_TEST_TIMEOUT", "600000")

      config = load(active_sanitizers: MapSet.new(["alubsan"]))

      assert config.timeout_factor == 3
      assert config.test_timeout == 600_000 * 3
    end
  end

  describe "cluster fields" do
    test "defaults" do
      config = load()

      assert config.cluster_agents == 3
      assert config.cluster_dbservers == 3
      assert config.cluster_coordinators == 1
      assert config.cluster_replication_factor == 2
    end

    test "env var overrides" do
      System.put_env("TOAST_CLUSTER_AGENTS", "5")
      System.put_env("TOAST_CLUSTER_DBSERVERS", "2")
      System.put_env("TOAST_CLUSTER_COORDINATORS", "3")
      System.put_env("TOAST_CLUSTER_REPLICATION_FACTOR", "4")

      config = load()

      assert config.cluster_agents == 5
      assert config.cluster_dbservers == 2
      assert config.cluster_coordinators == 3
      assert config.cluster_replication_factor == 4
    end

    test "keyword overrides" do
      config = load(cluster_agents: 10, cluster_dbservers: 5)

      assert config.cluster_agents == 10
      assert config.cluster_dbservers == 5
    end
  end

  describe "TOAST_API_VERSION env var" do
    test "parses integer version" do
      System.put_env("TOAST_API_VERSION", "2")
      assert load().api_version == 2
    end

    test "keeps string version" do
      System.put_env("TOAST_API_VERSION", "2.0")
      assert load().api_version == "2.0"
    end

    test "nil defaults to nil" do
      assert load().api_version == nil
    end

    test "keyword override" do
      System.put_env("TOAST_API_VERSION", "2")
      config = load(api_version: 3)
      assert config.api_version == 3
    end
  end

  describe "TOAST_DEBUGGER env var" do
    test "gdb string maps to :gdb atom" do
      System.put_env("TOAST_DEBUGGER", "gdb")
      assert load().debugger == :gdb
    end

    test "lldb string maps to :lldb atom" do
      System.put_env("TOAST_DEBUGGER", "lldb")
      assert load().debugger == :lldb
    end

    test "auto string maps to :auto atom" do
      System.put_env("TOAST_DEBUGGER", "auto")
      assert load().debugger == :auto
    end

    test "unrecognized values fall back to default :auto" do
      System.put_env("TOAST_DEBUGGER", "something")
      assert load().debugger == :auto
    end

    test "defaults to :auto" do
      assert load().debugger == :auto
    end

    test "keyword override" do
      System.put_env("TOAST_DEBUGGER", "gdb")
      config = load(debugger: :lldb)
      assert config.debugger == :lldb
    end
  end

  describe "TOAST_DUMP_AGENCY env var" do
    test "true string sets dump_agency_on_error to true" do
      System.put_env("TOAST_DUMP_AGENCY", "true")
      assert load().dump_agency_on_error == true
    end

    test "false string sets dump_agency_on_error to false" do
      System.put_env("TOAST_DUMP_AGENCY", "false")
      assert load().dump_agency_on_error == false
    end

    test "defaults to true when not set" do
      assert load().dump_agency_on_error == true
    end

    test "keyword override" do
      System.put_env("TOAST_DUMP_AGENCY", "true")
      config = load(dump_agency_on_error: false)
      assert config.dump_agency_on_error == false
    end
  end

  describe "TOAST_COREDUMP_TIMEOUT env var" do
    test "parses integer from string" do
      System.put_env("TOAST_COREDUMP_TIMEOUT", "60000")
      assert load().coredump_timeout == 60_000
    end

    test "defaults to 120_000 when not set" do
      assert load().coredump_timeout == 180_000
    end

    test "keyword override" do
      System.put_env("TOAST_COREDUMP_TIMEOUT", "60000")
      config = load(coredump_timeout: 30_000)
      assert config.coredump_timeout == 30_000
    end
  end

  describe ".toast.local.exs" do
    setup do
      dir = Path.join(System.tmp_dir!(), "config_test_#{:erlang.unique_integer([:positive])}")
      File.mkdir_p!(dir)
      path = Path.join(dir, ".toast.local.exs")
      on_exit(fn -> File.rm_rf!(dir) end)
      {:ok, dir: dir, local_path: path}
    end

    test "reads config from .toast.local.exs when present", %{dir: dir, local_path: path} do
      File.write!(path, "%{build_dir: \"/local/build\"}")

      config = load(local_config_dir: dir)
      assert config.build_dir == "/local/build"
    end

    test "env vars take precedence over .toast.local.exs", %{dir: dir, local_path: path} do
      File.write!(path, "%{build_dir: \"/local/build\"}")
      System.put_env("TOAST_BUILD_DIR", "/env/build")

      config = load(local_config_dir: dir)
      assert config.build_dir == "/env/build"
    end

    test "keyword opts take precedence over .toast.local.exs", %{dir: dir, local_path: path} do
      File.write!(path, "%{build_dir: \"/local/build\"}")

      config = load(build_dir: "/keyword/build", local_config_dir: dir)
      assert config.build_dir == "/keyword/build"
    end

    test "ignored when absent", %{dir: dir} do
      config = load(local_config_dir: dir)
      assert config.build_dir == nil
    end

    test "skipped when TOAST_CI=true", %{dir: dir, local_path: path} do
      File.write!(path, "%{build_dir: \"/local/build\"}")
      System.put_env("TOAST_CI", "true")

      config = load(local_config_dir: dir)
      assert config.build_dir == nil
    end

    test "reads dump_agency_on_error from .toast.local.exs", %{dir: dir, local_path: path} do
      File.write!(path, "%{dump_agency_on_error: false}")

      config = load(local_config_dir: dir)
      assert config.dump_agency_on_error == false
    end

    test "reads coredump_timeout from .toast.local.exs", %{dir: dir, local_path: path} do
      File.write!(path, "%{coredump_timeout: 60000}")

      config = load(local_config_dir: dir)
      assert config.coredump_timeout == 60_000
    end

    test "reads debugger from .toast.local.exs", %{dir: dir, local_path: path} do
      File.write!(path, "%{debugger: :gdb}")

      config = load(local_config_dir: dir)
      assert config.debugger == :gdb
    end

    test "env vars take precedence over .toast.local.exs for new keys", %{
      dir: dir,
      local_path: path
    } do
      File.write!(path, "%{dump_agency_on_error: false, coredump_timeout: 60000}")
      System.put_env("TOAST_DUMP_AGENCY", "true")
      System.put_env("TOAST_COREDUMP_TIMEOUT", "90000")

      config = load(local_config_dir: dir)
      assert config.dump_agency_on_error == true
      assert config.coredump_timeout == 90_000
    end

    test "invalid syntax logs warning and returns defaults", %{dir: dir, local_path: path} do
      File.write!(path, "this is not valid elixir {{{")

      config = load(local_config_dir: dir)
      assert config.build_dir == nil
      assert config.deployment_mode == :single_server
    end
  end

  describe "TOAST_SHUTDOWN_TIMEOUT env var" do
    test "parses integer from string" do
      System.put_env("TOAST_SHUTDOWN_TIMEOUT", "90000")

      assert load().shutdown_timeout == 90_000
    end

    test "factor is applied to shutdown_timeout" do
      System.put_env("TOAST_SHUTDOWN_TIMEOUT", "30000")

      config = load(active_sanitizers: MapSet.new(["asan"]))

      assert config.shutdown_timeout == 30_000 * 3
    end
  end

  describe "TOAST_COREDUMP_DIR env var" do
    test "reads coredump_dir from environment" do
      System.put_env("TOAST_COREDUMP_DIR", "/custom/cores")

      assert load().coredump_dir == "/custom/cores"
    end

    test "defaults to nil when not set" do
      assert load().coredump_dir == nil
    end

    test "keyword override takes precedence" do
      System.put_env("TOAST_COREDUMP_DIR", "/env/cores")

      config = load(coredump_dir: "/opt/cores")
      assert config.coredump_dir == "/opt/cores"
    end
  end

  describe "TOAST_RESULT_DIR env var" do
    test "reads result_dir from environment" do
      System.put_env("TOAST_RESULT_DIR", "/custom/results")

      assert load().result_dir == "/custom/results"
    end

    test "defaults to toast-results when not set" do
      assert load().result_dir == "toast-results"
    end

    test "keyword override takes precedence" do
      System.put_env("TOAST_RESULT_DIR", "/env/results")

      config = load(result_dir: "/opt/results")
      assert config.result_dir == "/opt/results"
    end
  end
end
