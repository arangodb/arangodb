defmodule Toast.Deployment.FactoryTest do
  use ExUnit.Case, async: false

  alias Toast.Config
  alias Toast.Deployment.Factory

  defp create_fake_repo(tmp_dir) do
    repo_root = Path.join(tmp_dir, "repo")
    bin_dir = Path.join([repo_root, "build", "bin"])
    arangod = Path.join(bin_dir, "arangod")

    File.mkdir_p!(bin_dir)
    File.mkdir_p!(Path.join(repo_root, "js"))
    File.mkdir_p!(Path.join(repo_root, "etc"))
    File.write!(arangod, "#!/bin/sh\n")
    File.chmod!(arangod, 0o755)

    %{repo_root: repo_root, bin_dir: bin_dir}
  end

  defp make_config(bin_dir, work_dir, opts \\ []) do
    Config.load(
      Keyword.merge(
        [bin_dir: bin_dir, work_dir: work_dir, show_server_logs: false],
        opts
      )
    )
  end

  describe "build_server_args/1" do
    test "show_server_logs false suppresses non-error output" do
      config = Config.load(show_server_logs: false, server_args: %{})
      assert Factory.build_server_args(config) == %{"log.output" => "-;all=error"}
    end

    test "show_server_logs true passes output through" do
      config = Config.load(show_server_logs: true, server_args: %{})
      assert Factory.build_server_args(config) == %{"log.output" => "-"}
    end

    test "custom server_args override defaults" do
      config = Config.load(show_server_logs: false, server_args: %{"log.output" => "custom", "extra" => "val"})
      result = Factory.build_server_args(config)

      assert result["log.output"] == "custom"
      assert result["extra"] == "val"
    end
  end

  describe "build_single_server/3" do
    setup do
      tmp_dir = Path.join(System.tmp_dir!(), "toast_factory_test_#{System.unique_integer([:positive])}")
      File.mkdir_p!(tmp_dir)
      on_exit(fn -> File.rm_rf!(tmp_dir) end)
      %{tmp_dir: tmp_dir}
    end

    test "returns correct spec structure", %{tmp_dir: tmp_dir} do
      %{bin_dir: bin_dir, repo_root: repo_root} = create_fake_repo(tmp_dir)
      work_dir = Path.join(tmp_dir, "work")
      config = make_config(bin_dir, work_dir)

      assert {:ok, spec} = Factory.build_single_server(config, "srv1", 8529)

      assert spec.id == "srv1"
      assert spec.executable == Path.join(bin_dir, "arangod")
      assert spec.port == 8529
      assert spec.env == []
      assert is_list(spec.args)
      assert spec.working_dir == repo_root
      assert spec.server_dir == Path.join(work_dir, "srv1")

      assert "--configuration" in spec.args
      assert "etc/testing/arangod-single.conf" in spec.args
      assert "--define" in spec.args
      assert "TOP_DIR=#{repo_root}" in spec.args
      assert "--server.endpoint" in spec.args
      assert "tcp://0.0.0.0:8529" in spec.args
    end

    test "creates data and app directories", %{tmp_dir: tmp_dir} do
      %{bin_dir: bin_dir} = create_fake_repo(tmp_dir)
      work_dir = Path.join(tmp_dir, "work")
      config = make_config(bin_dir, work_dir)

      assert {:ok, _spec} = Factory.build_single_server(config, "srv2", 8530)

      assert File.dir?(Path.join([work_dir, "srv2", "data"]))
      assert File.dir?(Path.join([work_dir, "srv2", "apps"]))
    end

    test "returns error when arangod is missing", %{tmp_dir: tmp_dir} do
      empty_bin = Path.join(tmp_dir, "empty_bin")
      File.mkdir_p!(empty_bin)
      work_dir = Path.join(tmp_dir, "work")
      config = make_config(empty_bin, work_dir)

      assert {:error, msg} = Factory.build_single_server(config, "srv3", 8531)
      assert msg =~ "arangod not found"
    end
  end
end
