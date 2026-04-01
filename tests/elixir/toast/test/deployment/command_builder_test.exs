defmodule Toast.Deployment.CommandBuilderTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.CommandBuilder

  @repo_root "/fake/repo"
  @paths %{data_dir: "/tmp/data", app_dir: "/tmp/apps", log_file: "/tmp/log"}

  describe "build_args/3 config file" do
    test "single uses single config" do
      result =
        CommandBuilder.build_args(%{role: :single, port: 8529, args: %{}}, @paths, @repo_root)

      assert config_value(result) == "etc/testing/arangod-single.conf"
    end

    test "agent uses agent config" do
      result =
        CommandBuilder.build_args(%{role: :agent, port: 8529, args: %{}}, @paths, @repo_root)

      assert config_value(result) == "etc/testing/arangod-agent.conf"
    end

    test "coordinator uses coordinator config" do
      result =
        CommandBuilder.build_args(
          %{role: :coordinator, port: 8529, args: %{}},
          @paths,
          @repo_root
        )

      assert config_value(result) == "etc/testing/arangod-coordinator.conf"
    end

    test "dbserver uses dbserver config" do
      result =
        CommandBuilder.build_args(%{role: :dbserver, port: 8529, args: %{}}, @paths, @repo_root)

      assert config_value(result) == "etc/testing/arangod-dbserver.conf"
    end
  end

  describe "build_args/3 role args" do
    test "single includes storage engine args" do
      result =
        CommandBuilder.build_args(%{role: :single, port: 8529, args: %{}}, @paths, @repo_root)

      assert has_flag_value?(result, "--server.storage-engine", "rocksdb")
    end

    test "agent includes agency args" do
      result =
        CommandBuilder.build_args(%{role: :agent, port: 8529, args: %{}}, @paths, @repo_root)

      assert has_flag_value?(result, "--agency.activate", "true")
      assert has_flag_value?(result, "--agency.supervision", "true")
    end

    test "coordinator includes cluster args" do
      result =
        CommandBuilder.build_args(
          %{role: :coordinator, port: 8529, args: %{}},
          @paths,
          @repo_root
        )

      assert has_flag_value?(result, "--cluster.create-waits-for-sync-replication", "false")
      assert has_flag_value?(result, "--cluster.write-concern", "1")
    end

    test "dbserver includes same cluster args as coordinator" do
      db =
        CommandBuilder.build_args(%{role: :dbserver, port: 8529, args: %{}}, @paths, @repo_root)

      coord =
        CommandBuilder.build_args(
          %{role: :coordinator, port: 8529, args: %{}},
          @paths,
          @repo_root
        )

      cluster_args = fn args ->
        args
        |> Enum.chunk_every(2)
        |> Enum.filter(fn [flag | _] -> String.starts_with?(flag, "--cluster.") end)
      end

      assert cluster_args.(db) == cluster_args.(coord)
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
               "--log.level",
               "crash=info",
               "--log.use-json-format",
               "true",
               "--log.ids",
               "true",
               "--log.process",
               "true",
               "--server.storage-engine",
               "rocksdb"
             ]
    end

    test "includes custom args at the end" do
      spec = %{role: :single, port: 8529, args: %{"log.level" => "debug"}}

      result = CommandBuilder.build_args(spec, @paths, @repo_root)

      assert "--log.level" in result
      assert "debug" in result

      # Custom log.level should come after the base crash=info and role args
      log_indices =
        result
        |> Enum.with_index()
        |> Enum.filter(fn {v, _i} -> v == "--log.level" end)
        |> Enum.map(&elem(&1, 1))

      # Two --log.level flags: base (crash=info) and custom (debug)
      assert length(log_indices) == 2
      [_base_idx, custom_idx] = log_indices
      assert Enum.at(result, custom_idx + 1) == "debug"

      role_idx = Enum.find_index(result, &(&1 == "--server.storage-engine"))
      assert custom_idx > role_idx
    end
  end

  describe "build_args/3 custom args" do
    test "empty custom args add nothing beyond base and role args" do
      result =
        CommandBuilder.build_args(%{role: :single, port: 8529, args: %{}}, @paths, @repo_root)

      refute "--key" in result
    end

    test "string values" do
      result =
        CommandBuilder.build_args(
          %{role: :single, port: 8529, args: %{"key" => "val"}},
          @paths,
          @repo_root
        )

      assert has_flag_value?(result, "--key", "val")
    end

    test "list values produce repeated flags" do
      args = %{"agency.endpoint" => ["tcp://a:1", "tcp://b:2"]}

      result =
        CommandBuilder.build_args(%{role: :single, port: 8529, args: args}, @paths, @repo_root)

      pairs =
        result
        |> Enum.chunk_every(2, 1, :discard)
        |> Enum.filter(fn [flag, _] -> flag == "--agency.endpoint" end)

      assert pairs == [["--agency.endpoint", "tcp://a:1"], ["--agency.endpoint", "tcp://b:2"]]
    end

    test "nil values are skipped" do
      result =
        CommandBuilder.build_args(
          %{role: :single, port: 8529, args: %{"key" => nil}},
          @paths,
          @repo_root
        )

      refute "--key" in result
    end

    test "keys are sorted for deterministic ordering" do
      args = %{"zebra" => "1", "alpha" => "2", "middle" => "3"}

      result =
        CommandBuilder.build_args(%{role: :single, port: 8529, args: args}, @paths, @repo_root)

      custom_start = Enum.find_index(result, &(&1 == "--alpha"))
      custom = Enum.drop(result, custom_start)
      assert custom == ["--alpha", "2", "--middle", "3", "--zebra", "1"]
    end

    test "mixed types are converted to strings" do
      args = %{"bool" => true, "number" => 42, "string" => "hello"}

      result =
        CommandBuilder.build_args(%{role: :single, port: 8529, args: args}, @paths, @repo_root)

      assert has_flag_value?(result, "--bool", "true")
      assert has_flag_value?(result, "--number", "42")
      assert has_flag_value?(result, "--string", "hello")
    end
  end

  defp config_value(args) do
    idx = Enum.find_index(args, &(&1 == "--configuration"))
    Enum.at(args, idx + 1)
  end

  defp has_flag_value?(args, flag, value) do
    args
    |> Enum.chunk_every(2, 1, :discard)
    |> Enum.any?(fn [f, v] -> f == flag and v == value end)
  end
end
