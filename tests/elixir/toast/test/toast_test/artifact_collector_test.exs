defmodule ToastTest.ArtifactCollectorTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.ServerInstance
  alias ToastTest.ArtifactCollector

  defp make_server(id, opts) do
    %ServerInstance{
      id: id,
      role: Keyword.get(opts, :role, :single),
      server_dir: Keyword.get(opts, :server_dir),
      pid: Keyword.get(opts, :pid)
    }
  end

  defp create_tmp_dir do
    dir =
      Path.join(
        System.tmp_dir!(),
        "artifact_collector_test_#{System.unique_integer([:positive])}"
      )

    File.mkdir_p!(dir)

    on_exit(fn -> File.rm_rf!(dir) end)

    dir
  end

  describe "collect/2" do
    test "empty servers map returns empty map" do
      assert ArtifactCollector.collect(%{}) == %{}
    end

    test "server with no artifacts returns empty lists" do
      dir = create_tmp_dir()
      server = make_server("s1", server_dir: dir)

      result = ArtifactCollector.collect(%{"s1" => server})

      assert %{"s1" => artifacts} = result
      assert artifacts.server == server
      assert artifacts.coredump_paths == []
      assert artifacts.sanitizer_files == []
    end

    test "server with nil server_dir is skipped" do
      server = make_server("s1", server_dir: nil)

      result = ArtifactCollector.collect(%{"s1" => server})

      assert result == %{}
    end

    test "discovers sanitizer log files" do
      dir = create_tmp_dir()

      # Create sanitizer log files with > 10 bytes
      alubsan_path = Path.join(dir, "alubsan.log.12345")
      tsan_path = Path.join(dir, "tsan.log.67890")
      File.write!(alubsan_path, String.duplicate("x", 20))
      File.write!(tsan_path, String.duplicate("y", 20))

      server = make_server("s1", server_dir: dir)
      result = ArtifactCollector.collect(%{"s1" => server})

      assert %{"s1" => artifacts} = result
      sanitizer_files = Enum.sort(artifacts.sanitizer_files)
      assert sanitizer_files == Enum.sort([alubsan_path, tsan_path])
    end

    test "filters out small sanitizer files (<=10 bytes)" do
      dir = create_tmp_dir()

      small_file = Path.join(dir, "alubsan.log.111")
      large_file = Path.join(dir, "alubsan.log.222")
      File.write!(small_file, "tiny")
      File.write!(large_file, String.duplicate("x", 20))

      server = make_server("s1", server_dir: dir)
      result = ArtifactCollector.collect(%{"s1" => server})

      assert %{"s1" => artifacts} = result
      assert artifacts.sanitizer_files == [large_file]
    end

    test "exactly 10 bytes is filtered out" do
      dir = create_tmp_dir()

      edge_file = Path.join(dir, "tsan.log.333")
      File.write!(edge_file, String.duplicate("x", 10))

      server = make_server("s1", server_dir: dir)
      result = ArtifactCollector.collect(%{"s1" => server})

      assert %{"s1" => artifacts} = result
      assert artifacts.sanitizer_files == []
    end

    test "11 bytes passes the filter" do
      dir = create_tmp_dir()

      file = Path.join(dir, "tsan.log.444")
      File.write!(file, String.duplicate("x", 11))

      server = make_server("s1", server_dir: dir)
      result = ArtifactCollector.collect(%{"s1" => server})

      assert %{"s1" => artifacts} = result
      assert artifacts.sanitizer_files == [file]
    end

    test "multiple servers each get their own artifacts" do
      dir1 = create_tmp_dir()
      dir2 = create_tmp_dir()

      file1 = Path.join(dir1, "alubsan.log.100")
      file2 = Path.join(dir2, "tsan.log.200")
      File.write!(file1, String.duplicate("a", 20))
      File.write!(file2, String.duplicate("b", 20))

      servers = %{
        "s1" => make_server("s1", server_dir: dir1),
        "s2" => make_server("s2", server_dir: dir2)
      }

      result = ArtifactCollector.collect(servers)

      assert map_size(result) == 2
      assert result["s1"].sanitizer_files == [file1]
      assert result["s2"].sanitizer_files == [file2]
    end

    test "non-sanitizer files are not collected" do
      dir = create_tmp_dir()

      File.write!(Path.join(dir, "arangod.log"), String.duplicate("x", 100))
      File.write!(Path.join(dir, "random.txt"), String.duplicate("x", 100))

      server = make_server("s1", server_dir: dir)
      result = ArtifactCollector.collect(%{"s1" => server})

      assert result["s1"].sanitizer_files == []
    end

    test "merges current pid and pid_history for coredump discovery" do
      dir = create_tmp_dir()

      server = make_server("s1", server_dir: dir, pid: 42)
      pid_history = %{"s1" => [10, 20]}

      result = ArtifactCollector.collect(%{"s1" => server}, pid_history)

      # We can't easily verify the pids passed to Coredump.discover,
      # but we verify the structure is correct and no crash occurs
      assert %{"s1" => artifacts} = result
      assert artifacts.coredump_paths == []
      assert artifacts.server == server
    end

    test "works with pid_history but no current pid" do
      dir = create_tmp_dir()

      server = make_server("s1", server_dir: dir, pid: nil)
      pid_history = %{"s1" => [10, 20]}

      result = ArtifactCollector.collect(%{"s1" => server}, pid_history)

      assert %{"s1" => artifacts} = result
      assert artifacts.coredump_paths == []
    end

    test "works with current pid but no pid_history" do
      dir = create_tmp_dir()

      server = make_server("s1", server_dir: dir, pid: 42)

      result = ArtifactCollector.collect(%{"s1" => server})

      assert %{"s1" => artifacts} = result
      assert artifacts.coredump_paths == []
    end
  end
end
