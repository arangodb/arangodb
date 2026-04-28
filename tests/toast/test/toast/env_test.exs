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

defmodule Toast.EnvTest do
  use ExUnit.Case, async: false

  setup do
    # Create a fake rr executable so validation passes
    tmp_dir = Path.join(System.tmp_dir!(), "toast_env_test_#{System.unique_integer([:positive])}")
    File.mkdir_p!(tmp_dir)
    on_exit(fn -> File.rm_rf!(tmp_dir) end)
    rr_path = Toast.PathTestHelpers.create_fake_executable("rr", tmp_dir)

    %{rr_path: rr_path}
  end

  defp load(opts \\ []) do
    Toast.Env.load(Keyword.put_new(opts, :local_config_dir, "/nonexistent"))
  end

  describe "load/1 returns config map" do
    test "contains essential config keys" do
      config = load()
      assert is_map(config)
      assert Map.has_key?(config, :build_dir)
      assert Map.has_key?(config, :deployment_mode)
      assert Map.has_key?(config, :test_timeout)
    end

    test "defaults" do
      config = load()
      assert config.deployment_mode == :single_server
      assert config.test_timeout == 300_000
      assert config.global_timeout == 3_600_000
      assert config.startup_timeout == 60_000
      assert config.shutdown_timeout == 60_000
      assert config.coredump_timeout == 180_000
      assert config.cluster_agents == 3
      assert config.cluster_dbservers == 3
      assert config.cluster_coordinators == 1
      assert config.cluster_replication_factor == 2
      assert config.show_server_logs == false
      assert config.keep_data == false
      assert config.ci == false
      assert config.debugger == :auto
      assert config.dump_agency_on_error == true
      assert config.result_dir == "toast-results"
      assert config.ssl == false
    end
  end

  describe "ssl option" do
    test "TOAST_SSL=true enables ssl" do
      System.put_env("TOAST_SSL", "true")
      on_exit(fn -> System.delete_env("TOAST_SSL") end)

      config = load()
      assert config.ssl == true
    end

    test "opts override TOAST_SSL env var" do
      System.put_env("TOAST_SSL", "true")
      on_exit(fn -> System.delete_env("TOAST_SSL") end)

      config = load(ssl: false)
      assert config.ssl == false
    end
  end

  describe "rr option parsing" do
    test "rr nil when not specified" do
      config = load()
      assert config.rr == nil
    end

    test "rr 'default' resolves to :single for single_server mode" do
      config = load(rr: "default")
      assert config.rr == MapSet.new([:single])
    end

    test "rr 'default' resolves to dbserver,coordinator for cluster mode" do
      config = load(rr: "default", deployment_mode: :cluster)
      assert config.rr == MapSet.new([:dbserver, :coordinator])
    end

    test "rr 'all' from opts" do
      config = load(rr: "all")
      assert config.rr == MapSet.new([:single, :agent, :dbserver, :coordinator])
    end

    test "rr specific roles from opts" do
      config = load(rr: "dbserver,coordinator")
      assert config.rr == MapSet.new([:dbserver, :coordinator])
    end

    test "rr single role from opts" do
      config = load(rr: "dbserver")
      assert config.rr == MapSet.new([:dbserver])
    end

    test "rr from TOAST_RR env var" do
      System.put_env("TOAST_RR", "dbserver,coordinator")
      on_exit(fn -> System.delete_env("TOAST_RR") end)

      config = load()
      assert config.rr == MapSet.new([:dbserver, :coordinator])
    end

    test "rr opts override env var" do
      System.put_env("TOAST_RR", "all")
      on_exit(fn -> System.delete_env("TOAST_RR") end)

      config = load(rr: "dbserver")
      assert config.rr == MapSet.new([:dbserver])
    end

    test "rr invalid role raises" do
      assert_raise ArgumentError, ~r/invalid rr role/, fn ->
        load(rr: "bogus")
      end
    end

    test "rr_path points to the fake executable when rr is active", %{rr_path: rr_path} do
      config = load(rr: "default")
      assert config.rr_path == rr_path
    end

    test "rr_path is nil when rr is not active" do
      config = load()
      assert config.rr_path == nil
    end

    test "rr raises when rr executable not found" do
      prev_path = System.get_env("PATH")
      System.put_env("PATH", "/usr/bin:/bin")
      on_exit(fn -> System.put_env("PATH", prev_path) end)

      assert_raise ArgumentError, ~r/rr.*not found in PATH/, fn ->
        load(rr: "default")
      end
    end
  end

  describe "timeout factor" do
    test "no rr, no sanitizer: timeout_factor defaults to 1" do
      config = load()
      assert config.timeout_factor == 1
    end

    test "auto-sets timeout_factor to 10 when rr is active" do
      config = load(rr: "default")
      assert config.timeout_factor == 10
    end

    test "explicit timeout_factor overrides rr auto-factor" do
      config = load(rr: "default", timeout_factor: 5)
      assert config.timeout_factor == 5
    end

    test "rr factor wins over sanitizer default" do
      config = load(rr: "default", sanitizer_override: "alubsan")
      assert config.timeout_factor == 10
    end

    test "sanitizer without rr sets timeout_factor to 3" do
      config = load(sanitizer_override: "alubsan")
      assert config.timeout_factor == 3
    end

    test "rr factor 10 multiplies all timeouts" do
      config = load(rr: "default")

      assert config.timeout_factor == 10
      assert config.test_timeout == 3_000_000
      assert config.global_timeout == 36_000_000
      assert config.startup_timeout == 600_000
      assert config.shutdown_timeout == 600_000
      assert config.coredump_timeout == 1_800_000
    end

    test "explicit factor multiplies all timeouts" do
      config = load(timeout_factor: 5)

      assert config.timeout_factor == 5
      assert config.test_timeout == 1_500_000
      assert config.global_timeout == 18_000_000
      assert config.startup_timeout == 300_000
      assert config.shutdown_timeout == 300_000
      assert config.coredump_timeout == 900_000
    end

    test "sanitizer factor 3 multiplies all timeouts" do
      config = load(sanitizer_override: "tsan")

      assert config.timeout_factor == 3
      assert config.test_timeout == 900_000
      assert config.global_timeout == 10_800_000
      assert config.startup_timeout == 180_000
      assert config.shutdown_timeout == 180_000
      assert config.coredump_timeout == 540_000
    end
  end

  describe "apply!/1" do
    setup do
      original = Application.get_all_env(:toast)
      on_exit(fn -> restore_app_env(original) end)
      :ok
    end

    test "writes config to Application env and returns :ok" do
      config = load(deployment_mode: :cluster)
      assert Toast.Env.apply!(config) == :ok
      assert Application.get_env(:toast, :deployment_mode) == :cluster
      assert Application.get_env(:toast, :__env_loaded__) == true
    end

    test "skips nil values" do
      config = load()
      # coredump_dir is nil by default (no TOAST_COREDUMP_DIR, no local config)
      assert config.coredump_dir == nil

      # Ensure key is not set before apply
      Application.delete_env(:toast, :coredump_dir)
      Toast.Env.apply!(config)

      # nil values should not be written to Application env
      assert Application.get_env(:toast, :coredump_dir) == nil
    end
  end

  describe "loaded?/0" do
    setup do
      original = Application.get_all_env(:toast)
      on_exit(fn -> restore_app_env(original) end)
      :ok
    end

    test "returns false before apply!/1 and true after" do
      # Application startup calls apply!, so __env_loaded__ is already true.
      # Explicitly clear it to test the false -> true transition.
      Application.delete_env(:toast, :__env_loaded__)
      assert Toast.Env.loaded?() == false

      config = load()
      Toast.Env.apply!(config)
      assert Toast.Env.loaded?() == true
    end
  end

  describe "memory_budget option" do
    test "defaults to auto-detected system memory" do
      config = load()
      # On any Linux machine, this should be a positive integer
      assert is_integer(config.memory_budget) and config.memory_budget > 0
    end

    test "TOAST_MEMORY_BUDGET env var overrides auto-detection" do
      System.put_env("TOAST_MEMORY_BUDGET", "4294967296")
      on_exit(fn -> System.delete_env("TOAST_MEMORY_BUDGET") end)

      config = load()
      assert config.memory_budget == 4_294_967_296
    end

    test "opts override env var" do
      System.put_env("TOAST_MEMORY_BUDGET", "4294967296")
      on_exit(fn -> System.delete_env("TOAST_MEMORY_BUDGET") end)

      config = load(memory_budget: 8_589_934_592)
      assert config.memory_budget == 8_589_934_592
    end
  end

  describe "protocol option" do
    test "defaults to :http1" do
      config = load()
      assert config.protocol == :http1
    end

    test "opts override default" do
      config = load(protocol: :http2)
      assert config.protocol == :http2
    end

    test "TOAST_PROTOCOL env var" do
      System.put_env("TOAST_PROTOCOL", "http2")
      on_exit(fn -> System.delete_env("TOAST_PROTOCOL") end)

      config = load()
      assert config.protocol == :http2
    end

    test "TOAST_PROTOCOL accepts h2 alias" do
      System.put_env("TOAST_PROTOCOL", "h2")
      on_exit(fn -> System.delete_env("TOAST_PROTOCOL") end)

      config = load()
      assert config.protocol == :http2
    end

    test "TOAST_PROTOCOL accepts h1 alias" do
      System.put_env("TOAST_PROTOCOL", "h1")
      on_exit(fn -> System.delete_env("TOAST_PROTOCOL") end)

      config = load()
      assert config.protocol == :http1
    end

    test "opts override TOAST_PROTOCOL env var" do
      System.put_env("TOAST_PROTOCOL", "http2")
      on_exit(fn -> System.delete_env("TOAST_PROTOCOL") end)

      config = load(protocol: :http1)
      assert config.protocol == :http1
    end

    test "invalid TOAST_PROTOCOL raises" do
      System.put_env("TOAST_PROTOCOL", "bogus")
      on_exit(fn -> System.delete_env("TOAST_PROTOCOL") end)

      assert_raise ArgumentError, ~r/Invalid TOAST_PROTOCOL/, fn ->
        load()
      end
    end
  end

  describe "error paths" do
    test "invalid deployment mode raises" do
      System.put_env("TOAST_DEPLOYMENT_MODE", "bogus")
      on_exit(fn -> System.delete_env("TOAST_DEPLOYMENT_MODE") end)

      assert_raise ArgumentError, ~r/Invalid TOAST_DEPLOYMENT_MODE/, fn ->
        load()
      end
    end

    test "invalid positive integer env var raises" do
      System.put_env("TOAST_TEST_TIMEOUT", "not_a_number")
      on_exit(fn -> System.delete_env("TOAST_TEST_TIMEOUT") end)

      assert_raise ArgumentError, ~r/must be a positive integer/, fn ->
        load()
      end
    end

    test "zero value for positive integer env var raises" do
      System.put_env("TOAST_TEST_TIMEOUT", "0")
      on_exit(fn -> System.delete_env("TOAST_TEST_TIMEOUT") end)

      assert_raise ArgumentError, ~r/must be a positive integer/, fn ->
        load()
      end
    end

    test "negative value for positive integer env var raises" do
      System.put_env("TOAST_TEST_TIMEOUT", "-5")
      on_exit(fn -> System.delete_env("TOAST_TEST_TIMEOUT") end)

      assert_raise ArgumentError, ~r/must be a positive integer/, fn ->
        load()
      end
    end
  end

  defp restore_app_env(original) do
    for {key, _} <- Application.get_all_env(:toast) do
      Application.delete_env(:toast, key)
    end

    for {key, val} <- original do
      Application.put_env(:toast, key, val)
    end
  end
end
