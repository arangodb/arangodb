defmodule ToastTest.Enrichment.CoredumpTest do
  use ExUnit.Case, async: true

  alias ToastTest.Enrichment.Coredump
  alias Toast.Diagnostics.Coredump.Report

  defp build_server(executable \\ "/usr/bin/arangod") do
    %Toast.Deployment.ServerInstance{
      id: "single1",
      role: :single,
      launch_spec: %Toast.Deployment.Factory.LaunchSpec{
        id: "single1",
        executable: executable,
        args: [],
        env: [],
        working_dir: "/tmp",
        server_dir: "/tmp",
        port: 8529,
        log_file: "/tmp/arangod.log"
      }
    }
  end

  describe "analyze/3" do
    test "returns error when no launch_spec" do
      server = %Toast.Deployment.ServerInstance{
        id: "single1",
        role: :single,
        launch_spec: nil
      }

      assert {:error, :no_executable} = Coredump.analyze("/tmp/core.1234", server)
    end

    test "transforms successful report into thread-list shape" do
      report = %Report{
        core_path: "/tmp/core.1234",
        binary_path: "/usr/bin/arangod",
        debugger: :gdb,
        signal: "SIGSEGV",
        faulting_address: "0xdeadbeef",
        crash_thread: 1,
        threads: [
          %{id: 1, frames: [%{function: "crash_func", file: "crash.cpp", line: 42}]},
          %{id: 2, frames: [%{function: "worker_func", file: "worker.cpp", line: 10}]}
        ]
      }

      server = build_server()

      result =
        Coredump.analyze("/tmp/core.1234", server,
          analyzer: fn _core, _bin, _opts -> {:ok, report} end
        )

      assert {:ok, enrichment} = result
      assert enrichment.signal == "SIGSEGV"
      assert length(enrichment.threads) == 2

      [thread1, thread2] = enrichment.threads
      assert thread1.thread_id == "1"
      assert thread1.backtrace =~ "crash_func"
      assert thread2.thread_id == "2"
      assert thread2.backtrace =~ "worker_func"
    end

    test "thread name is nil when not present in report" do
      report = %Report{
        core_path: "/tmp/core.1234",
        binary_path: "/usr/bin/arangod",
        debugger: :lldb,
        signal: nil,
        faulting_address: nil,
        crash_thread: nil,
        threads: [%{id: 5, frames: []}]
      }

      server = build_server()

      assert {:ok, enrichment} =
               Coredump.analyze("/tmp/core.1234", server,
                 analyzer: fn _, _, _ -> {:ok, report} end
               )

      assert [%{thread_id: "5", name: nil, backtrace: ""}] = enrichment.threads
      assert enrichment.signal == nil
    end

    test "thread name is extracted when present in report" do
      report = %Report{
        core_path: "/tmp/core.1234",
        binary_path: "/usr/bin/arangod",
        debugger: :gdb,
        signal: "SIGABRT",
        faulting_address: nil,
        crash_thread: 1,
        threads: [%{id: 1, name: "Scheduler", frames: []}]
      }

      server = build_server()

      assert {:ok, enrichment} =
               Coredump.analyze("/tmp/core.1234", server,
                 analyzer: fn _, _, _ -> {:ok, report} end
               )

      assert [%{thread_id: "1", name: "Scheduler"}] = enrichment.threads
    end

    test "propagates analyzer errors" do
      server = build_server()

      assert {:error, :no_debugger} =
               Coredump.analyze("/tmp/core.1234", server,
                 analyzer: fn _, _, _ -> {:error, :no_debugger} end
               )
    end

    test "passes options through to analyzer" do
      server = build_server()
      test_pid = self()

      Coredump.analyze("/tmp/core.1234", server,
        timeout: 5_000,
        analyzer: fn core, bin, opts ->
          send(test_pid, {:called, core, bin, opts})
          {:error, :test}
        end
      )

      assert_received {:called, "/tmp/core.1234", "/usr/bin/arangod", opts}
      assert Keyword.get(opts, :timeout) == 5_000
    end
  end
end
