defmodule Toast.Deployment.CommandBuilderTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.CommandBuilder

  @repo_root "/fake/repo"
  @paths %{data_dir: "/tmp/data", app_dir: "/tmp/apps", log_file: "/tmp/log"}

  describe "config_file/1" do
    test "single" do
      assert CommandBuilder.config_file(:single) == "etc/testing/arangod-single.conf"
    end

    test "agent" do
      assert CommandBuilder.config_file(:agent) == "etc/testing/arangod-agent.conf"
    end

    test "coordinator" do
      assert CommandBuilder.config_file(:coordinator) == "etc/testing/arangod-coordinator.conf"
    end

    test "dbserver" do
      assert CommandBuilder.config_file(:dbserver) == "etc/testing/arangod-dbserver.conf"
    end
  end

  describe "role_args/1" do
    test "single returns storage engine args" do
      assert CommandBuilder.role_args(:single) == ["--server.storage-engine", "rocksdb"]
    end

    test "agent returns agency args" do
      assert CommandBuilder.role_args(:agent) == [
               "--agency.activate",
               "true",
               "--agency.supervision",
               "true"
             ]
    end

    test "coordinator returns cluster args" do
      expected = [
        "--cluster.create-waits-for-sync-replication",
        "false",
        "--cluster.write-concern",
        "1"
      ]

      assert CommandBuilder.role_args(:coordinator) == expected
    end

    test "dbserver returns same cluster args as coordinator" do
      assert CommandBuilder.role_args(:dbserver) == CommandBuilder.role_args(:coordinator)
    end
  end

  describe "build_args/3" do
    test "single server produces all expected flags in order" do
      spec = %{role: :single, port: 8529, args: %{}}

      result = CommandBuilder.build_args(spec, @paths, @repo_root)

      assert result == [
               "--configuration",
               "etc/testing/arangod-single.conf",
               "--define",
               "TOP_DIR=#{@repo_root}",
               "--server.endpoint",
               "tcp://0.0.0.0:8529",
               "--database.directory",
               @paths.data_dir,
               "--javascript.app-path",
               @paths.app_dir,
               "--log.file",
               @paths.log_file,
               "--server.storage-engine",
               "rocksdb"
             ]
    end

    test "includes custom args at the end" do
      spec = %{role: :single, port: 8529, args: %{"log.level" => "debug"}}

      result = CommandBuilder.build_args(spec, @paths, @repo_root)

      assert "--log.level" in result
      assert "debug" in result

      log_idx = Enum.find_index(result, &(&1 == "--log.level"))
      assert Enum.at(result, log_idx + 1) == "debug"

      role_idx = Enum.find_index(result, &(&1 == "--server.storage-engine"))
      assert log_idx > role_idx
    end
  end

  describe "flatten_custom_args/1" do
    test "empty map returns empty list" do
      assert CommandBuilder.flatten_custom_args(%{}) == []
    end

    test "string values" do
      assert CommandBuilder.flatten_custom_args(%{"key" => "val"}) == ["--key", "val"]
    end

    test "list values produce repeated flags" do
      result = CommandBuilder.flatten_custom_args(%{"agency.endpoint" => ["tcp://a:1", "tcp://b:2"]})

      assert result == [
               "--agency.endpoint",
               "tcp://a:1",
               "--agency.endpoint",
               "tcp://b:2"
             ]
    end

    test "nil values are skipped" do
      assert CommandBuilder.flatten_custom_args(%{"key" => nil}) == []
    end

    test "keys are sorted for deterministic ordering" do
      args = %{"zebra" => "1", "alpha" => "2", "middle" => "3"}
      result = CommandBuilder.flatten_custom_args(args)

      assert result == ["--alpha", "2", "--middle", "3", "--zebra", "1"]
    end

    test "mixed types are converted to strings" do
      args = %{"bool" => true, "number" => 42, "string" => "hello"}
      result = CommandBuilder.flatten_custom_args(args)

      assert result == ["--bool", "true", "--number", "42", "--string", "hello"]
    end
  end
end
