defmodule Toast.Deployment.FactoryTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.{Config, Factory}

  defp create_fake_repo(tmp_dir) do
    repo_root = Path.join(tmp_dir, "repo")
    build_dir = Path.join(repo_root, "build")
    arangod = Path.join([repo_root, "build", "bin", "arangod"])

    File.mkdir_p!(Path.join([repo_root, "build", "bin"]))
    for dir <- ~w(arangod js etc), do: File.mkdir_p!(Path.join(repo_root, dir))
    File.write!(arangod, "#!/bin/sh\n")
    File.chmod!(arangod, 0o755)

    %{repo_root: repo_root, build_dir: build_dir}
  end

  defp make_config(build_dir, opts \\ []) do
    Config.new(Keyword.merge([build_dir: build_dir, show_server_logs: false], opts))
  end

  describe "build_single_server/2" do
    setup do
      tmp_dir =
        Path.join(System.tmp_dir!(), "toast_factory_test_#{System.unique_integer([:positive])}")

      File.mkdir_p!(tmp_dir)
      on_exit(fn -> File.rm_rf!(tmp_dir) end)
      %{tmp_dir: tmp_dir}
    end

    test "returns correct spec structure", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir, repo_root: repo_root} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")
      config = make_config(build_dir)

      deployment_dir = Path.join(base_dir, "srv1")
      assert {:ok, [spec]} = Factory.build_single_server(config, "srv1", deployment_dir)

      assert spec.id == "srv1"
      assert spec.role == :single
      assert spec.executable == Path.join([build_dir, "bin", "arangod"])
      assert is_integer(spec.port) and spec.port > 0
      assert is_list(spec.env)
      assert is_list(spec.args)
      assert spec.working_dir == repo_root
      assert spec.server_dir == deployment_dir

      assert "--configuration" in spec.args
      assert "etc/testing/arangod-single.conf" in spec.args
      assert "--define" in spec.args
      assert "TOP_DIR=#{repo_root}" in spec.args
      assert "--server.endpoint" in spec.args
      assert "tcp://0.0.0.0:#{spec.port}" in spec.args
    end

    test "creates data and app directories", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")
      config = make_config(build_dir)

      deployment_dir = Path.join(base_dir, "srv2")
      assert {:ok, [_spec]} = Factory.build_single_server(config, "srv2", deployment_dir)

      assert File.dir?(Path.join(deployment_dir, "data"))
      assert File.dir?(Path.join(deployment_dir, "apps"))
    end

    test "returns error when arangod is missing", %{tmp_dir: tmp_dir} do
      empty_build = Path.join(tmp_dir, "empty_build")
      File.mkdir_p!(empty_build)
      base_dir = Path.join(tmp_dir, "work")
      config = make_config(empty_build)

      deployment_dir = Path.join(base_dir, "srv3")
      assert {:error, msg} = Factory.build_single_server(config, "srv3", deployment_dir)
      assert msg =~ "arangod not found"
    end

    test "show_server_logs false suppresses non-error output", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")
      config = make_config(build_dir, show_server_logs: false)

      deployment_dir = Path.join(base_dir, "srv-log1")
      assert {:ok, [spec]} = Factory.build_single_server(config, "srv-log1", deployment_dir)
      assert not Enum.any?(spec.args, &(&1 == "--log.output"))
    end

    test "show_server_logs true passes output through", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")
      config = make_config(build_dir, show_server_logs: true)

      deployment_dir = Path.join(base_dir, "srv-log2")
      assert {:ok, [spec]} = Factory.build_single_server(config, "srv-log2", deployment_dir)
      assert has_flag_value?(spec.args, "--log.output", "+")
    end

    test "custom server_args override defaults", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir,
          show_server_logs: false,
          server_args: %{"log.output" => "custom", "extra" => "val"}
        )

      deployment_dir = Path.join(base_dir, "srv-custom")

      assert {:ok, [spec]} =
               Factory.build_single_server(config, "srv-custom", deployment_dir)

      assert has_flag_value?(spec.args, "--log.output", "custom")
      assert has_flag_value?(spec.args, "--extra", "val")
    end
  end

  describe "build_cluster/3" do
    setup do
      tmp_dir =
        Path.join(System.tmp_dir!(), "toast_cluster_test_#{System.unique_integer([:positive])}")

      File.mkdir_p!(tmp_dir)
      on_exit(fn -> File.rm_rf!(tmp_dir) end)
      %{tmp_dir: tmp_dir}
    end

    test "returns flat list ordered by deploy order", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:ok, specs} = Factory.build_cluster(config, "test-cluster", deployment_dir)

      assert length(specs) == 7

      agents = Enum.filter(specs, &(&1.role == :agent))
      dbservers = Enum.filter(specs, &(&1.role == :dbserver))
      coordinators = Enum.filter(specs, &(&1.role == :coordinator))

      assert length(agents) == 3
      assert length(dbservers) == 3
      assert length(coordinators) == 1

      # Agents come first, then dbservers, then coordinators
      agent_indices = Enum.map(agents, &Enum.find_index(specs, fn s -> s.id == &1.id end))
      dbserver_indices = Enum.map(dbservers, &Enum.find_index(specs, fn s -> s.id == &1.id end))
      coord_indices = Enum.map(coordinators, &Enum.find_index(specs, fn s -> s.id == &1.id end))

      assert Enum.max(agent_indices) < Enum.min(dbserver_indices)
      assert Enum.max(dbserver_indices) < Enum.min(coord_indices)
    end

    test "server IDs follow naming convention", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:ok, specs} = Factory.build_cluster(config, "test-cluster", deployment_dir)

      agents = Enum.filter(specs, &(&1.role == :agent))
      dbservers = Enum.filter(specs, &(&1.role == :dbserver))
      coordinators = Enum.filter(specs, &(&1.role == :coordinator))

      agent_ids = Enum.map(agents, & &1.id)
      assert agent_ids == ["test-cluster-agent-0", "test-cluster-agent-1", "test-cluster-agent-2"]

      dbserver_ids = Enum.map(dbservers, & &1.id)

      assert dbserver_ids == [
               "test-cluster-dbserver-0",
               "test-cluster-dbserver-1",
               "test-cluster-dbserver-2"
             ]

      coordinator_ids = Enum.map(coordinators, & &1.id)
      assert coordinator_ids == ["test-cluster-coordinator-0"]
    end

    test "agent specs include agency-specific args", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:ok, specs} = Factory.build_cluster(config, "test-cluster", deployment_dir)

      agents = Enum.filter(specs, &(&1.role == :agent))
      agent_ports = Enum.map(agents, & &1.port)

      for {agent, port} <- Enum.zip(agents, agent_ports) do
        args = agent.args

        assert "--agency.activate" in args
        assert "true" in args
        assert "--agency.supervision" in args
        assert "--agency.size" in args
        assert "3" in args
        assert "--agency.my-address" in args
        assert "tcp://127.0.0.1:#{port}" in args

        endpoint_count =
          args
          |> Enum.chunk_every(2, 1, :discard)
          |> Enum.count(fn [flag, _val] -> flag == "--agency.endpoint" end)

        assert endpoint_count == 3
      end
    end

    test "dbserver specs include cluster args with all agency endpoints", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:ok, specs} = Factory.build_cluster(config, "test-cluster", deployment_dir)

      dbservers = Enum.filter(specs, &(&1.role == :dbserver))

      for dbserver <- dbservers do
        args = dbserver.args

        assert "--cluster.my-role" in args
        assert "PRIMARY" in args
        assert "--cluster.my-address" in args
        assert "tcp://127.0.0.1:#{dbserver.port}" in args

        endpoint_count =
          args
          |> Enum.chunk_every(2, 1, :discard)
          |> Enum.count(fn [flag, _val] -> flag == "--cluster.agency-endpoint" end)

        assert endpoint_count == 3
      end
    end

    test "coordinator specs include cluster and foxx args", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:ok, specs} = Factory.build_cluster(config, "test-cluster", deployment_dir)

      coordinators = Enum.filter(specs, &(&1.role == :coordinator))

      for coordinator <- coordinators do
        args = coordinator.args

        assert "--cluster.my-role" in args
        assert "COORDINATOR" in args
        assert "--cluster.default-replication-factor" in args
        assert "2" in args
        assert "--foxx.force-update-on-startup" in args
        assert "true" in args
      end
    end

    test "all ports are unique", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:ok, specs} = Factory.build_cluster(config, "test-cluster", deployment_dir)

      all_ports = Enum.map(specs, & &1.port)
      assert length(all_ports) == length(Enum.uniq(all_ports))
    end

    test "all working dirs point to repo root", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir, repo_root: repo_root} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:ok, specs} = Factory.build_cluster(config, "test-cluster", deployment_dir)

      for spec <- specs do
        assert spec.working_dir == repo_root
      end
    end

    test "all specs have correct role field", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(build_dir, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:ok, specs} = Factory.build_cluster(config, "test-cluster", deployment_dir)

      for spec <- specs do
        assert spec.role in [:agent, :dbserver, :coordinator]
      end
    end

    test "returns error when arangod is missing", %{tmp_dir: tmp_dir} do
      empty_build = Path.join(tmp_dir, "empty_build")
      File.mkdir_p!(empty_build)
      base_dir = Path.join(tmp_dir, "work")

      config =
        make_config(empty_build, cluster: [agents: 3, dbservers: 3, coordinators: 1])

      deployment_dir = Path.join(base_dir, "test-cluster")
      assert {:error, msg} = Factory.build_cluster(config, "test-cluster", deployment_dir)
      assert msg =~ "arangod not found"
    end
  end

  describe "memory budget distribution" do
    setup do
      tmp_dir =
        Path.join(System.tmp_dir!(), "toast_mem_test_#{System.unique_integer([:positive])}")

      File.mkdir_p!(tmp_dir)
      on_exit(fn -> File.rm_rf!(tmp_dir) end)
      %{tmp_dir: tmp_dir}
    end

    @memory_env_var "ARANGODB_OVERRIDE_DETECTED_TOTAL_MEMORY"

    defp memory_env(spec) do
      Enum.find_value(spec.env, fn
        {@memory_env_var, val} -> String.to_integer(val)
        _ -> nil
      end)
    end

    test "single server gets full memory budget", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      total = 8_000_000_000
      config = make_config(build_dir, memory_budget: total)

      deployment_dir = Path.join(tmp_dir, "work")
      assert {:ok, [spec]} = Factory.build_single_server(config, "mem1", deployment_dir)
      assert memory_env(spec) == total
    end

    test "single server with nil memory has no override", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      config = make_config(build_dir, memory_budget: nil)

      deployment_dir = Path.join(tmp_dir, "work")
      assert {:ok, [spec]} = Factory.build_single_server(config, "mem2", deployment_dir)
      assert memory_env(spec) == nil
    end

    test "cluster distributes 8/69/23% across roles", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      total = 10_000_000_000

      config =
        make_config(build_dir,
          memory_budget: total,
          cluster: [agents: 1, dbservers: 2, coordinators: 1]
        )

      deployment_dir = Path.join(tmp_dir, "work")
      assert {:ok, specs} = Factory.build_cluster(config, "memc", deployment_dir)

      agents = Enum.filter(specs, &(&1.role == :agent))
      dbservers = Enum.filter(specs, &(&1.role == :dbserver))
      coordinators = Enum.filter(specs, &(&1.role == :coordinator))

      for spec <- agents, do: assert(memory_env(spec) == 800_000_000)
      for spec <- dbservers, do: assert(memory_env(spec) == 3_450_000_000)
      for spec <- coordinators, do: assert(memory_env(spec) == 2_300_000_000)
    end

    test "cluster with nil memory has no override", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)

      config =
        make_config(build_dir,
          memory_budget: nil,
          cluster: [agents: 1, dbservers: 1, coordinators: 1]
        )

      deployment_dir = Path.join(tmp_dir, "work")
      assert {:ok, specs} = Factory.build_cluster(config, "memc2", deployment_dir)

      for spec <- specs, do: assert(memory_env(spec) == nil)
    end
  end

  describe "rr wrapping" do
    setup do
      tmp_dir =
        Path.join(System.tmp_dir!(), "toast_rr_test_#{System.unique_integer([:positive])}")

      File.mkdir_p!(tmp_dir)
      on_exit(fn -> File.rm_rf!(tmp_dir) end)
      fake_rr = Toast.PathTestHelpers.create_fake_executable("rr", tmp_dir)

      %{tmp_dir: tmp_dir, fake_rr: fake_rr}
    end

    test "single server: wraps executable when rr includes :single", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      rr_path = System.find_executable("rr")
      config = make_config(build_dir, rr: MapSet.new([:single]), rr_path: rr_path)

      deployment_dir = Path.join(tmp_dir, "work-rr")
      assert {:ok, [spec]} = Factory.build_single_server(config, "rr1", deployment_dir)

      arangod = Path.join([build_dir, "bin", "arangod"])
      assert spec.executable == System.find_executable("rr")
      assert ["record", "-o", trace_dir, ^arangod | _rest] = spec.args
      assert String.ends_with?(trace_dir, "/rr-trace")
      assert File.dir?(trace_dir)
    end

    test "single server: no wrapping when rr is nil", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      config = make_config(build_dir)

      deployment_dir = Path.join(tmp_dir, "work-norr")
      assert {:ok, [spec]} = Factory.build_single_server(config, "norr1", deployment_dir)

      assert spec.executable == Path.join([build_dir, "bin", "arangod"])
      refute "record" in spec.args
    end

    test "single server: no wrapping when rr excludes :single", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)
      config = make_config(build_dir, rr: MapSet.new([:dbserver]))

      deployment_dir = Path.join(tmp_dir, "work-norr2")
      assert {:ok, [spec]} = Factory.build_single_server(config, "norr2", deployment_dir)

      assert spec.executable == Path.join([build_dir, "bin", "arangod"])
      refute "record" in spec.args
    end

    test "cluster: wraps only specified roles", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)

      config =
        make_config(build_dir,
          cluster: [agents: 1, dbservers: 2, coordinators: 1],
          rr: MapSet.new([:dbserver, :coordinator]),
          rr_path: System.find_executable("rr")
        )

      deployment_dir = Path.join(tmp_dir, "work-cluster-rr")
      assert {:ok, specs} = Factory.build_cluster(config, "crr", deployment_dir)

      rr_path = System.find_executable("rr")
      arangod = Path.join([build_dir, "bin", "arangod"])

      for spec <- specs do
        if spec.role in [:dbserver, :coordinator] do
          assert spec.executable == rr_path,
                 "expected #{spec.role} #{spec.id} to be wrapped with rr"

          assert ["record", "-o", _trace_dir, ^arangod | _] = spec.args
        else
          assert spec.executable == arangod,
                 "expected #{spec.role} #{spec.id} NOT to be wrapped with rr"

          refute "record" in spec.args
        end
      end
    end

    test "cluster: default roles wrap dbserver and coordinator", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)

      config =
        make_config(build_dir,
          cluster: [agents: 1, dbservers: 1, coordinators: 1],
          rr: MapSet.new([:dbserver, :coordinator]),
          rr_path: System.find_executable("rr")
        )

      deployment_dir = Path.join(tmp_dir, "work-cluster-rr-default")
      assert {:ok, specs} = Factory.build_cluster(config, "crrd", deployment_dir)

      rr_path = System.find_executable("rr")
      arangod = Path.join([build_dir, "bin", "arangod"])

      for spec <- specs do
        if spec.role in [:dbserver, :coordinator] do
          assert spec.executable == rr_path
        else
          assert spec.executable == arangod
        end
      end
    end

    test "single server: wraps when role is in rr set", %{tmp_dir: tmp_dir} do
      %{build_dir: build_dir} = create_fake_repo(tmp_dir)

      config =
        make_config(build_dir,
          rr: MapSet.new([:single]),
          rr_path: System.find_executable("rr")
        )

      deployment_dir = Path.join(tmp_dir, "work-rr-default-single")
      assert {:ok, [spec]} = Factory.build_single_server(config, "rrds", deployment_dir)

      assert spec.executable == System.find_executable("rr")
    end
  end

  defp has_flag_value?(args, flag, value) do
    args
    |> Enum.chunk_every(2, 1, :discard)
    |> Enum.any?(fn [f, v] -> f == flag and v == value end)
  end
end
