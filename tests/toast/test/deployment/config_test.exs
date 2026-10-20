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

defmodule Toast.Deployment.ConfigTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.{ClusterOpts, Config}

  # Clean up any toast application env keys we set during tests.
  @config_env_keys [
    :build_dir,
    :show_server_logs,
    :server_args,
    :active_sanitizers,
    :sanitizer_override,
    :timeout_factor,
    :startup_timeout,
    :shutdown_timeout,
    :api_version,
    :memory_budget,
    :rr,
    :rr_path,
    :authentication,
    :jwt_algorithm,
    :ssl
  ]

  @cluster_env_keys [
    :cluster_agents,
    :cluster_dbservers,
    :cluster_coordinators,
    :cluster_replication_factor,
    :coordinator_args,
    :dbserver_args,
    :agent_args
  ]

  setup do
    saved =
      for key <- @config_env_keys ++ @cluster_env_keys, into: %{} do
        {key, Application.get_env(:toast, key)}
      end

    on_exit(fn ->
      for {key, val} <- saved do
        case val do
          nil -> Application.delete_env(:toast, key)
          val -> Application.put_env(:toast, key, val)
        end
      end
    end)

    # Clear all keys so tests start from a clean slate.
    for key <- @config_env_keys ++ @cluster_env_keys do
      Application.delete_env(:toast, key)
    end

    :ok
  end

  describe "Config.new/0 and Config.new/1" do
    test "returns struct with defaults when no overrides and no app env" do
      config = Config.new()

      assert %Config{} = config
      assert config.build_dir == nil
      assert config.show_server_logs == false
      assert config.server_args == %{}
      assert config.active_sanitizers == MapSet.new()
      assert config.sanitizer_override == nil
      assert config.timeout_factor == 1
      assert config.startup_timeout == 60_000
      assert config.shutdown_timeout == 60_000
      assert config.api_version == nil
      assert config.cluster == nil
      assert config.rr == nil
      assert config.rr_path == nil
      assert config.authentication == false
      assert config.jwt_algorithm == :hmac
      assert config.ssl == false
    end

    test "reads defaults from application env" do
      Application.put_env(:toast, :build_dir, "/custom/build")
      Application.put_env(:toast, :show_server_logs, true)
      Application.put_env(:toast, :timeout_factor, 3)
      Application.put_env(:toast, :startup_timeout, 120_000)

      config = Config.new()

      assert config.build_dir == "/custom/build"
      assert config.show_server_logs == true
      assert config.timeout_factor == 3
      assert config.startup_timeout == 120_000
      # Unset keys still get struct defaults
      assert config.shutdown_timeout == 60_000
    end

    test "explicit overrides take precedence over application env" do
      Application.put_env(:toast, :build_dir, "/from/env")
      Application.put_env(:toast, :timeout_factor, 5)

      config = Config.new(build_dir: "/from/override", timeout_factor: 2)

      assert config.build_dir == "/from/override"
      assert config.timeout_factor == 2
    end

    test "cluster: nil produces single server config" do
      config = Config.new(cluster: nil)
      assert config.cluster == nil
    end

    test "cluster: false produces single server config" do
      config = Config.new(cluster: false)
      assert config.cluster == nil
    end

    test "cluster: true produces cluster config with defaults" do
      config = Config.new(cluster: true)
      assert %ClusterOpts{} = config.cluster
      assert config.cluster.agents == 3
      assert config.cluster.dbservers == 3
      assert config.cluster.coordinators == 1
      assert config.cluster.replication_factor == 2
    end

    test "cluster: keyword list overrides specific cluster fields" do
      config = Config.new(cluster: [dbservers: 5, coordinators: 3])

      assert %ClusterOpts{} = config.cluster
      assert config.cluster.dbservers == 5
      assert config.cluster.coordinators == 3
      # Non-overridden fields keep defaults
      assert config.cluster.agents == 3
      assert config.cluster.replication_factor == 2
    end

    test "cluster option does not leak into struct fields" do
      config = Config.new(cluster: true, build_dir: "/some/path")

      assert %ClusterOpts{} = config.cluster
      assert config.build_dir == "/some/path"
    end

    test "raises on unknown override keys" do
      assert_raise KeyError, fn ->
        Config.new(nonexistent_field: "boom")
      end
    end

    test "authentication: true is accepted" do
      config = Config.new(authentication: true)
      assert config.authentication == true
      assert config.jwt_algorithm == :hmac
    end

    test "authentication: true with :ecdsa algorithm" do
      config = Config.new(authentication: true, jwt_algorithm: :ecdsa)
      assert config.authentication == true
      assert config.jwt_algorithm == :ecdsa
    end

    test "raises when jwt_algorithm is set without authentication" do
      assert_raise ArgumentError, ~r/jwt_algorithm.*requires authentication/, fn ->
        Config.new(jwt_algorithm: :ecdsa)
      end
    end

    test "raises when jwt_algorithm is set with authentication: false" do
      assert_raise ArgumentError, ~r/jwt_algorithm.*requires authentication/, fn ->
        Config.new(authentication: false, jwt_algorithm: :ecdsa)
      end
    end

    test "raises when jwt_algorithm comes from app env without authentication" do
      Application.put_env(:toast, :jwt_algorithm, :ecdsa)

      assert_raise ArgumentError, ~r/jwt_algorithm.*requires authentication/, fn ->
        Config.new()
      end
    end

    test "ssl: true is accepted" do
      config = Config.new(ssl: true)
      assert config.ssl == true
    end

    test "ssl defaults to false" do
      config = Config.new()
      assert config.ssl == false
    end

    test "ssl reads from application env" do
      Application.put_env(:toast, :ssl, true)
      config = Config.new()
      assert config.ssl == true
    end
  end

  describe "Config.cluster?/1" do
    test "returns false when cluster is nil" do
      config = %Config{cluster: nil}
      refute Config.cluster?(config)
    end

    test "returns true when cluster is a ClusterOpts struct" do
      config = %Config{cluster: %ClusterOpts{}}
      assert Config.cluster?(config)
    end
  end

  describe "Config.mode/1" do
    test "returns :single_server when cluster is nil" do
      config = %Config{cluster: nil}
      assert Config.mode(config) == :single_server
    end

    test "returns :cluster when cluster is a ClusterOpts struct" do
      config = %Config{cluster: %ClusterOpts{}}
      assert Config.mode(config) == :cluster
    end
  end

  describe "ClusterOpts.new/1" do
    test "returns struct with defaults when no overrides and no app env" do
      opts = ClusterOpts.new()

      assert %ClusterOpts{} = opts
      assert opts.agents == 3
      assert opts.dbservers == 3
      assert opts.coordinators == 1
      assert opts.replication_factor == 2
      assert opts.coordinator_args == %{}
      assert opts.dbserver_args == %{}
      assert opts.agent_args == %{}
    end

    test "new(true) is equivalent to new([])" do
      opts_true = ClusterOpts.new(true)
      opts_empty = ClusterOpts.new([])

      assert opts_true == opts_empty
    end

    test "reads defaults from application env" do
      Application.put_env(:toast, :cluster_agents, 5)
      Application.put_env(:toast, :cluster_dbservers, 7)
      Application.put_env(:toast, :cluster_coordinators, 4)
      Application.put_env(:toast, :cluster_replication_factor, 3)
      Application.put_env(:toast, :coordinator_args, %{"log.level" => "debug"})

      opts = ClusterOpts.new()

      assert opts.agents == 5
      assert opts.dbservers == 7
      assert opts.coordinators == 4
      assert opts.replication_factor == 3
      assert opts.coordinator_args == %{"log.level" => "debug"}
      # Unset keys still get hardcoded defaults
      assert opts.dbserver_args == %{}
      assert opts.agent_args == %{}
    end

    test "explicit overrides take precedence over application env" do
      Application.put_env(:toast, :cluster_dbservers, 10)
      Application.put_env(:toast, :cluster_coordinators, 8)

      opts = ClusterOpts.new(dbservers: 2, coordinators: 1)

      assert opts.dbservers == 2
      assert opts.coordinators == 1
    end

    test "per-role args can be overridden" do
      custom_args = %{"log.level" => "trace"}
      opts = ClusterOpts.new(coordinator_args: custom_args, agent_args: custom_args)

      assert opts.coordinator_args == custom_args
      assert opts.agent_args == custom_args
      assert opts.dbserver_args == %{}
    end

    test "raises on unknown override keys" do
      assert_raise KeyError, fn ->
        ClusterOpts.new(nonexistent_field: "boom")
      end
    end
  end
end
