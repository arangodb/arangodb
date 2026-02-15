defmodule Toast.ConfigTest do
  use ExUnit.Case, async: false

  alias Toast.Config

  @env_vars ~w(
    TOAST_BIN_DIR
    TOAST_WORK_DIR
    TOAST_DEPLOYMENT_MODE
    TOAST_SHOW_SERVER_LOGS
    TOAST_STARTUP_TIMEOUT
    TOAST_SHUTDOWN_TIMEOUT
    TOAST_CLUSTER_AGENTS
    TOAST_CLUSTER_DBSERVERS
    TOAST_CLUSTER_COORDINATORS
    TOAST_CLUSTER_REPLICATION_FACTOR
  )

  setup do
    saved = Map.new(@env_vars, fn var -> {var, System.get_env(var)} end)

    on_exit(fn ->
      for {var, val} <- saved do
        if val, do: System.put_env(var, val), else: System.delete_env(var)
      end
    end)

    clear_env_vars()
    :ok
  end

  defp clear_env_vars do
    Enum.each(@env_vars, &System.delete_env/1)
  end

  describe "defaults" do
    test "returns struct with expected defaults" do
      config = Config.load()

      assert %Config{} = config
      assert config.bin_dir == nil
      assert config.deployment_mode == :single_server
      assert config.show_server_logs == false
      assert config.server_args == %{}
      assert config.startup_timeout == 60_000
      assert config.shutdown_timeout == 30_000
    end

    test "work_dir has unique default under tmp_dir" do
      config = Config.load()

      assert String.starts_with?(config.work_dir, System.tmp_dir!())
      assert config.work_dir =~ ~r/toast_\d+$/
    end
  end

  describe "TOAST_BIN_DIR env var" do
    test "reads bin_dir from environment" do
      System.put_env("TOAST_BIN_DIR", "/custom/bin")

      assert Config.load().bin_dir == "/custom/bin"
    end
  end

  describe "TOAST_WORK_DIR env var" do
    test "reads work_dir from environment" do
      System.put_env("TOAST_WORK_DIR", "/custom/work")

      assert Config.load().work_dir == "/custom/work"
    end
  end

  describe "TOAST_DEPLOYMENT_MODE env var" do
    test "cluster string maps to :cluster atom" do
      System.put_env("TOAST_DEPLOYMENT_MODE", "cluster")

      assert Config.load().deployment_mode == :cluster
    end

    test "other values default to :single_server" do
      System.put_env("TOAST_DEPLOYMENT_MODE", "something_else")

      assert Config.load().deployment_mode == :single_server
    end

    test "nil defaults to :single_server" do
      assert Config.load().deployment_mode == :single_server
    end
  end

  describe "TOAST_SHOW_SERVER_LOGS env var" do
    test "true string maps to true" do
      System.put_env("TOAST_SHOW_SERVER_LOGS", "true")

      assert Config.load().show_server_logs == true
    end

    test "other values default to false" do
      System.put_env("TOAST_SHOW_SERVER_LOGS", "false")

      assert Config.load().show_server_logs == false
    end

    test "nil defaults to false" do
      assert Config.load().show_server_logs == false
    end
  end

  describe "TOAST_STARTUP_TIMEOUT env var" do
    test "parses integer from string" do
      System.put_env("TOAST_STARTUP_TIMEOUT", "120000")

      assert Config.load().startup_timeout == 120_000
    end
  end

  describe "keyword overrides" do
    test "take precedence over env vars" do
      System.put_env("TOAST_BIN_DIR", "/env/bin")
      System.put_env("TOAST_WORK_DIR", "/env/work")
      System.put_env("TOAST_DEPLOYMENT_MODE", "cluster")
      System.put_env("TOAST_SHOW_SERVER_LOGS", "true")
      System.put_env("TOAST_STARTUP_TIMEOUT", "120000")

      config =
        Config.load(
          bin_dir: "/opt/bin",
          work_dir: "/opt/work",
          deployment_mode: :single_server,
          show_server_logs: false,
          startup_timeout: 5_000
        )

      assert config.bin_dir == "/opt/bin"
      assert config.work_dir == "/opt/work"
      assert config.deployment_mode == :single_server
      assert config.show_server_logs == false
      assert config.startup_timeout == 5_000
    end
  end

  describe "pos_int validation" do
    test "zero raises ArgumentError" do
      System.put_env("TOAST_STARTUP_TIMEOUT", "0")

      assert_raise ArgumentError, ~r/must be a positive integer/, fn ->
        Config.load()
      end
    end

    test "negative value raises ArgumentError" do
      System.put_env("TOAST_CLUSTER_AGENTS", "-1")

      assert_raise ArgumentError, ~r/must be a positive integer/, fn ->
        Config.load()
      end
    end

    test "non-numeric value raises ArgumentError" do
      System.put_env("TOAST_STARTUP_TIMEOUT", "abc")

      assert_raise ArgumentError, fn ->
        Config.load()
      end
    end
  end

  describe "server_args" do
    test "passes through from opts" do
      args = %{"log.level" => "debug"}
      config = Config.load(server_args: args)

      assert config.server_args == %{"log.level" => "debug"}
    end
  end

  describe "cluster fields" do
    test "defaults" do
      config = Config.load()

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

      config = Config.load()

      assert config.cluster_agents == 5
      assert config.cluster_dbservers == 2
      assert config.cluster_coordinators == 3
      assert config.cluster_replication_factor == 4
    end

    test "keyword overrides" do
      config = Config.load(cluster_agents: 10, cluster_dbservers: 5)

      assert config.cluster_agents == 10
      assert config.cluster_dbservers == 5
    end
  end
end
