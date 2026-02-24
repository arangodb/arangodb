defmodule Toast.Utils.FilesystemTest do
  use ExUnit.Case, async: false

  alias Toast.Utils.Filesystem

  defp unique_tmp_dir do
    dir = Path.join(System.tmp_dir!(), "toast_fs_test_#{System.unique_integer([:positive])}")
    File.mkdir_p!(dir)

    on_exit(fn -> File.rm_rf(dir) end)

    dir
  end

  describe "create_server_dirs/2" do
    test "creates data/ and apps/ directories" do
      work_dir = unique_tmp_dir()

      assert {:ok, dirs} = Filesystem.create_server_dirs(work_dir, "srv1")

      assert File.dir?(dirs.data_dir)
      assert File.dir?(dirs.app_dir)
    end

    test "returns correct paths structure" do
      work_dir = unique_tmp_dir()

      {:ok, dirs} = Filesystem.create_server_dirs(work_dir, "srv1")

      assert dirs.base_dir == Path.join(work_dir, "srv1")
      assert dirs.data_dir == Path.join(work_dir, "srv1/data")
      assert dirs.app_dir == Path.join(work_dir, "srv1/apps")
      assert dirs.log_file == Path.join(work_dir, "srv1/log")
    end

    test "log_file path is returned but file is not created" do
      work_dir = unique_tmp_dir()

      {:ok, dirs} = Filesystem.create_server_dirs(work_dir, "srv1")

      refute File.exists?(dirs.log_file)
    end
  end

  describe "cleanup_server_dirs/1" do
    test "removes created directories" do
      work_dir = unique_tmp_dir()
      {:ok, dirs} = Filesystem.create_server_dirs(work_dir, "srv1")

      assert File.dir?(dirs.base_dir)
      assert Filesystem.cleanup_server_dirs(dirs.base_dir) == :ok
      refute File.exists?(dirs.base_dir)
    end

    test "returns :ok for nonexistent directory" do
      assert Filesystem.cleanup_server_dirs("/tmp/toast_nonexistent_#{System.unique_integer([:positive])}") == :ok
    end
  end

  describe "find_arangod/1" do
    test "finds arangod in given build_dir" do
      dir = unique_tmp_dir()
      bin_dir = Path.join(dir, "bin")
      File.mkdir_p!(bin_dir)
      arangod_path = Path.join(bin_dir, "arangod")
      File.touch!(arangod_path)
      File.chmod!(arangod_path, 0o755)

      expected_path = Path.join([dir, "bin", "arangod"])
      assert {:ok, ^expected_path} = Filesystem.find_arangod(dir)
    end

    test "returns error with nil when arangod is not in PATH" do
      original_path = System.get_env("PATH")
      empty_dir = unique_tmp_dir()
      System.put_env("PATH", empty_dir)

      on_exit(fn -> System.put_env("PATH", original_path) end)

      assert {:error, "arangod not found in PATH"} = Filesystem.find_arangod(nil)
    end

    test "returns error with helpful message for wrong directory" do
      dir = unique_tmp_dir()

      assert {:error, msg} = Filesystem.find_arangod(dir)
      assert msg =~ "arangod not found at"
      assert msg =~ dir
    end
  end

  describe "find_repository_root/1" do
    test "finds root via non-bin directory one level below root" do
      root = unique_tmp_dir()
      for dir <- ~w(arangod js etc), do: File.mkdir_p!(Path.join(root, dir))

      build_dir = Path.join(root, "mybuild")
      File.mkdir_p!(build_dir)

      assert {:ok, ^root} = Filesystem.find_repository_root(build_dir)
    end

    test "finds root from bin_dir in build/bin/ structure" do
      root = unique_tmp_dir()
      for dir <- ~w(arangod js etc), do: File.mkdir_p!(Path.join(root, dir))

      build_dir = Path.join(root, "build")
      File.mkdir_p!(build_dir)

      assert {:ok, ^root} = Filesystem.find_repository_root(build_dir)
    end

    test "returns error when build_dir candidate has no js/ and etc/ and cwd has none either" do
      empty_dir = unique_tmp_dir()
      build_dir = Path.join(empty_dir, "sub")
      File.mkdir_p!(build_dir)

      assert {:error, "repository root not found"} =
               Filesystem.find_repository_root(build_dir, cwd: empty_dir)
    end
  end
end
