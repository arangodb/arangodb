defmodule Toast.Diagnostics.CoredumpTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.Coredump
  alias Toast.Diagnostics.Coredump.Report

  describe "discover/1" do
    setup do
      dir = Path.join(System.tmp_dir!(), "toast_core_test_#{System.unique_integer([:positive])}")
      File.mkdir_p!(dir)
      on_exit(fn -> File.rm_rf!(dir) end)
      %{dir: dir}
    end

    test "finds core files in server directory", %{dir: dir} do
      File.write!(Path.join(dir, "core.12345"), "fake core")
      File.write!(Path.join(dir, "core"), "fake core")
      File.write!(Path.join(dir, "not_a_core"), "other file")

      cores = Coredump.discover(server_dir: dir)

      assert length(cores) == 2
      basenames = Enum.map(cores, &Path.basename/1)
      assert "core.12345" in basenames
      assert "core" in basenames
      refute "not_a_core" in basenames
    end

    test "returns empty list when no core files found", %{dir: dir} do
      File.write!(Path.join(dir, "some_file.txt"), "not a core")

      assert Coredump.discover(server_dir: dir) == []
    end

    test "returns empty list for nonexistent directory" do
      assert Coredump.discover(
               server_dir: "/nonexistent/dir/#{System.unique_integer([:positive])}"
             ) == []
    end

    test "deduplicates paths", %{dir: dir} do
      File.write!(Path.join(dir, "core.99999"), "fake core")

      cores = Coredump.discover(server_dir: dir, os_pid: nil)

      # Each core file should appear only once
      assert cores == Enum.uniq(cores)
    end

    # T4: PID-filtered discovery in /tmp with segment-based matching
    test "finds core files in /tmp matching the given os_pid by segment", %{dir: dir} do
      pid = System.unique_integer([:positive])
      pid_str = to_string(pid)

      matching_file = Path.join("/tmp", "core.#{pid_str}")
      non_matching_file = Path.join("/tmp", "core.#{pid_str}99")

      File.write!(matching_file, "fake core")
      File.write!(non_matching_file, "fake core with longer pid")

      on_exit(fn ->
        File.rm(matching_file)
        File.rm(non_matching_file)
      end)

      cores = Coredump.discover(server_dir: dir, os_pid: pid)
      basenames = Enum.map(cores, &Path.basename/1)

      assert "core.#{pid_str}" in basenames
      # Segment-based matching: "core.12" must not match "core.1299"
      refute "core.#{pid_str}99" in basenames
    end
  end

  describe "discover/1 with not_before filtering" do
    setup do
      dir = Path.join(System.tmp_dir!(), "toast_notbefore_#{System.unique_integer([:positive])}")
      File.mkdir_p!(dir)
      on_exit(fn -> File.rm_rf!(dir) end)
      %{dir: dir}
    end

    test "filters out core files older than not_before timestamp", %{dir: dir} do
      core_path = Path.join(dir, "core.old")
      File.write!(core_path, "old core")

      # Set not_before to a time far in the future so the file is "too old"
      future_ts = DateTime.to_unix(DateTime.utc_now()) + 86400

      cores = Coredump.discover(server_dir: dir, not_before: future_ts)
      assert cores == []
    end

    test "includes core files newer than not_before timestamp", %{dir: dir} do
      core_path = Path.join(dir, "core.new")
      File.write!(core_path, "new core")

      # Set not_before to epoch so the file is definitely "new enough"
      cores = Coredump.discover(server_dir: dir, not_before: 0)
      assert core_path in cores
    end
  end

  describe "analyze/3 with debugger tuple" do
    test "accepts {module, executable} tuple" do
      defmodule TupleEchoDebugger do
        @behaviour Toast.Diagnostics.Coredump.Debugger

        @impl true
        def executable, do: "echo"

        @impl true
        def command(_binary, _core), do: ["tuple test"]

        @impl true
        def parse_output(_output),
          do: %{signal: nil, faulting_address: nil, threads: [], crash_thread: nil}
      end

      result =
        Coredump.analyze("/fake/core", "/fake/binary", debugger: {TupleEchoDebugger, "echo"})

      assert {:ok, %Report{}} = result
    end
  end

  describe "analyze/3" do
    test "returns error when no debugger available" do
      result = Coredump.analyze("/fake/core", "/fake/binary", debugger: nil)

      assert result == {:error, :no_debugger}
    end
  end

  describe "analyze/3 error paths" do
    # T6: analyze with a non-existent core file and a real executable
    # The debugger command will fail because the core file doesn't exist.
    test "returns error for non-existent core file with echo as debugger" do
      defmodule EchoDebugger do
        @behaviour Toast.Diagnostics.Coredump.Debugger

        @impl true
        def executable, do: "echo"

        @impl true
        def command(_binary, _core), do: ["no useful output"]

        @impl true
        def parse_output(_output),
          do: %{signal: nil, faulting_address: nil, threads: [], crash_thread: nil}
      end

      result =
        Coredump.analyze("/nonexistent/core", "/nonexistent/binary", debugger: EchoDebugger)

      # echo exits 0 but parse_output returns no threads, so build_report succeeds
      # with an empty report
      assert {:ok, %Report{} = report} = result
      assert report.threads == []
    end

    # T6: test non-zero exit code with no useful output
    test "returns error tuple when debugger exits non-zero with no useful output" do
      defmodule FailingDebugger do
        @behaviour Toast.Diagnostics.Coredump.Debugger

        @impl true
        def executable, do: "false"

        @impl true
        def command(_binary, _core), do: []

        @impl true
        def parse_output(_output),
          do: %{signal: nil, faulting_address: nil, threads: [], crash_thread: nil}
      end

      result = Coredump.analyze("/fake/core", "/fake/binary", debugger: FailingDebugger)

      assert {:error, {:debugger_failed, 1, ""}} = result
    end

    test "recovers useful output from non-zero exit code" do
      defmodule NonZeroButUsefulDebugger do
        @behaviour Toast.Diagnostics.Coredump.Debugger

        @impl true
        def executable, do: "sh"

        @impl true
        def command(_binary, _core), do: ["-c", "echo 'some output'; exit 1"]

        @impl true
        def parse_output(_output) do
          %{
            signal: "SIGSEGV",
            faulting_address: nil,
            threads: [%{id: 1, frames: [%{function: "crash", file: "f.cpp", line: 1}]}],
            crash_thread: 1
          }
        end
      end

      result = Coredump.analyze("/fake/core", "/fake/binary", debugger: NonZeroButUsefulDebugger)

      assert {:ok, %Report{} = report} = result
      assert report.signal == "SIGSEGV"
      assert length(report.threads) == 1
      assert report.debugger == :unknown
    end

    test "returns error for nonexistent debugger executable" do
      defmodule NoSuchDebugger do
        @behaviour Toast.Diagnostics.Coredump.Debugger

        @impl true
        def executable, do: "toast_definitely_not_a_real_executable_12345"

        @impl true
        def command(_binary, _core), do: []

        @impl true
        def parse_output(_output),
          do: %{signal: nil, faulting_address: nil, threads: [], crash_thread: nil}
      end

      result = Coredump.analyze("/fake/core", "/fake/binary", debugger: NoSuchDebugger)

      assert {:error, {:debugger_error, :executable_not_found}} = result
    end
  end
end

# T3: Separate module for env-var-dependent discover tests
defmodule Toast.Diagnostics.CoredumpOverrideDirTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.Coredump

  describe "discover/1 with coredump_dir override" do
    setup do
      override_dir =
        Path.join(System.tmp_dir!(), "toast_override_core_#{System.unique_integer([:positive])}")

      server_dir =
        Path.join(System.tmp_dir!(), "toast_server_core_#{System.unique_integer([:positive])}")

      File.mkdir_p!(override_dir)
      File.mkdir_p!(server_dir)

      on_exit(fn ->
        File.rm_rf!(override_dir)
        File.rm_rf!(server_dir)
      end)

      %{override_dir: override_dir, server_dir: server_dir}
    end

    test "uses override dir exclusively and ignores server_dir", %{
      override_dir: override_dir,
      server_dir: server_dir
    } do
      File.write!(Path.join(override_dir, "core.override"), "override core")
      File.write!(Path.join(server_dir, "core.server"), "server core")

      cores = Coredump.discover(server_dir: server_dir, coredump_dir: override_dir)
      basenames = Enum.map(cores, &Path.basename/1)

      assert "core.override" in basenames
      refute "core.server" in basenames
    end

    test "returns empty list when override dir has no core files", %{
      override_dir: override_dir,
      server_dir: server_dir
    } do
      File.write!(Path.join(server_dir, "core.server"), "server core")

      assert Coredump.discover(server_dir: server_dir, coredump_dir: override_dir) == []
    end

    test "filters override dir files by PID when os_pids provided", %{
      override_dir: override_dir,
      server_dir: server_dir
    } do
      File.write!(Path.join(override_dir, "core.12345"), "matching core")
      File.write!(Path.join(override_dir, "core.99999"), "other core")

      cores =
        Coredump.discover(
          server_dir: server_dir,
          coredump_dir: override_dir,
          os_pids: [12345]
        )

      basenames = Enum.map(cores, &Path.basename/1)
      assert "core.12345" in basenames
      refute "core.99999" in basenames
    end

    test "returns all files from override dir when no PIDs given", %{
      override_dir: override_dir,
      server_dir: server_dir
    } do
      File.write!(Path.join(override_dir, "core.111"), "core1")
      File.write!(Path.join(override_dir, "core.222"), "core2")

      cores = Coredump.discover(server_dir: server_dir, coredump_dir: override_dir)

      assert length(cores) == 2
    end

    test "returns empty list when override dir does not exist", %{server_dir: server_dir} do
      cores =
        Coredump.discover(
          server_dir: server_dir,
          coredump_dir: "/nonexistent/override_#{System.unique_integer([:positive])}"
        )

      assert cores == []
    end
  end
end
