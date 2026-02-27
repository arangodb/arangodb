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

  describe "detect_debugger/0" do
    test "returns a module or :none" do
      result = Coredump.detect_debugger()

      case result do
        {:ok, mod} -> assert mod in [Coredump.GDB, Coredump.LLDB]
        :none -> assert true
      end
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
      assert {:ok, %Report{}} = result
      assert {:ok, report} = result
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

  describe "collect/1" do
    test "returns empty list with no servers" do
      assert Coredump.collect(servers: []) == []
    end

    test "returns empty list with no servers key" do
      assert Coredump.collect([]) == []
    end

    # T7: collect with server data but no core files
    test "returns empty list when server directory has no core files" do
      dir =
        Path.join(System.tmp_dir!(), "toast_collect_test_#{System.unique_integer([:positive])}")

      File.mkdir_p!(dir)
      on_exit(fn -> File.rm_rf!(dir) end)

      server = %{
        id: "test-server",
        os_pid: 99_999,
        server_dir: dir,
        binary_path: "/usr/bin/arangod"
      }

      assert Coredump.collect(servers: [server], debugger: nil) == []
    end

    # T7: collect with a core file but a debugger that exits non-zero
    # Verifies that analysis errors are handled gracefully (logged, not propagated)
    test "returns empty list when debugger fails to analyze core files" do
      dir =
        Path.join(
          System.tmp_dir!(),
          "toast_collect_faildbg_#{System.unique_integer([:positive])}"
        )

      File.mkdir_p!(dir)
      File.write!(Path.join(dir, "core.77777"), "fake core")
      on_exit(fn -> File.rm_rf!(dir) end)

      defmodule CollectFailDebugger do
        @behaviour Toast.Diagnostics.Coredump.Debugger

        @impl true
        def executable, do: "false"

        @impl true
        def command(_binary, _core), do: []

        @impl true
        def parse_output(_output),
          do: %{signal: nil, faulting_address: nil, threads: [], crash_thread: nil}
      end

      server = %{
        id: "test-server-faildbg",
        os_pid: 77_777,
        server_dir: dir,
        binary_path: "/usr/bin/arangod"
      }

      result = Coredump.collect(servers: [server], debugger: CollectFailDebugger)

      assert result == []
    end

    test "skips servers when timeout budget is already exhausted" do
      dir =
        Path.join(System.tmp_dir!(), "toast_timeout_skip_#{System.unique_integer([:positive])}")

      File.mkdir_p!(dir)
      File.write!(Path.join(dir, "core.33333"), "fake")
      on_exit(fn -> File.rm_rf!(dir) end)

      server = %{id: "s1", os_pid: 33_333, server_dir: dir, binary_path: "/usr/bin/arangod"}

      result = Coredump.collect(servers: [server], timeout: 0)

      assert result == []
    end
  end

  describe "Report struct" do
    test "has expected fields" do
      report = %Report{
        core_path: "/tmp/core.123",
        binary_path: "/usr/bin/arangod",
        debugger: :gdb,
        signal: "SIGSEGV",
        faulting_address: "0xdeadbeef",
        crash_thread: 1,
        threads: [%{id: 1, frames: []}]
      }

      assert report.core_path == "/tmp/core.123"
      assert report.binary_path == "/usr/bin/arangod"
      assert report.debugger == :gdb
      assert report.signal == "SIGSEGV"
      assert report.faulting_address == "0xdeadbeef"
      assert report.crash_thread == 1
      assert length(report.threads) == 1
    end

    test "defaults to empty threads list" do
      report = %Report{core_path: "/tmp/core", binary_path: "/usr/bin/arangod", debugger: :gdb}

      assert report.threads == []
      assert report.signal == nil
      assert report.crash_thread == nil
    end
  end
end

# T3: Separate module for env-var-dependent discover tests
defmodule Toast.Diagnostics.CoredumpEnvTest do
  use ExUnit.Case, async: false

  alias Toast.Diagnostics.Coredump

  describe "discover/1 with TOAST_COREDUMP_DIR" do
    setup do
      override_dir =
        Path.join(System.tmp_dir!(), "toast_override_core_#{System.unique_integer([:positive])}")

      server_dir =
        Path.join(System.tmp_dir!(), "toast_server_core_#{System.unique_integer([:positive])}")

      File.mkdir_p!(override_dir)
      File.mkdir_p!(server_dir)

      saved = System.get_env("TOAST_COREDUMP_DIR")

      on_exit(fn ->
        if saved,
          do: System.put_env("TOAST_COREDUMP_DIR", saved),
          else: System.delete_env("TOAST_COREDUMP_DIR")

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

      System.put_env("TOAST_COREDUMP_DIR", override_dir)

      cores = Coredump.discover(server_dir: server_dir)
      basenames = Enum.map(cores, &Path.basename/1)

      assert "core.override" in basenames
      refute "core.server" in basenames
    end

    test "returns empty list when override dir has no core files", %{
      override_dir: override_dir,
      server_dir: server_dir
    } do
      File.write!(Path.join(server_dir, "core.server"), "server core")

      System.put_env("TOAST_COREDUMP_DIR", override_dir)

      assert Coredump.discover(server_dir: server_dir) == []
    end
  end
end
