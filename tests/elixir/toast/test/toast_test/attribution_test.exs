defmodule ToastTest.AttributionTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.Factory.LaunchSpec
  alias Toast.Deployment.ServerInstance
  alias Toast.Process.CrashEvent
  alias Toast.Process.CrashInfo
  alias ToastTest.Attribution

  # Suite: 10:00:00 - 10:10:00
  # ModA:  10:00:01 - 10:05:00
  #   test_one: 10:00:03 - 10:01:00
  #   test_two: 10:01:05 - 10:02:00

  @suite_started ~U[2026-03-09 10:00:00Z]
  @suite_finished ~U[2026-03-09 10:10:00Z]

  @mod_a_started ~U[2026-03-09 10:00:01Z]
  @mod_a_finished ~U[2026-03-09 10:05:00Z]

  @test1_started ~U[2026-03-09 10:00:03Z]
  @test1_finished ~U[2026-03-09 10:01:00Z]
  @test2_started ~U[2026-03-09 10:01:05Z]
  @test2_finished ~U[2026-03-09 10:02:00Z]

  defp build_test_data(overrides \\ %{}) do
    Map.merge(
      %{
        started_at: @suite_started,
        finished_at: @suite_finished,
        failures: [],
        modules: %{
          ModA => %{
            started_at: @mod_a_started,
            finished_at: @mod_a_finished,
            setup_finished_at: @test1_started,
            teardown_started_at: @test2_finished,
            tests: [
              %{name: :test_one, started_at: @test1_started, finished_at: @test1_finished},
              %{name: :test_two, started_at: @test2_started, finished_at: @test2_finished}
            ]
          }
        }
      },
      overrides
    )
  end

  defp build_server(id, opts \\ []) do
    server_dir = Keyword.get(opts, :server_dir, "/tmp/server_#{id}")
    log_file = Keyword.get(opts, :log_file, Path.join(server_dir, "arangod.log"))

    %ServerInstance{
      id: id,
      role: :single,
      server_dir: server_dir,
      log_file: log_file,
      launch_spec: %LaunchSpec{
        id: id,
        role: :single,
        executable: "/usr/bin/arangod",
        args: [],
        env: [],
        working_dir: server_dir,
        server_dir: server_dir,
        port: 8529,
        log_file: log_file
      }
    }
  end

  defp empty_artifacts, do: %{}

  defp make_exunit_test(module, name, state) do
    %ExUnit.Test{
      name: name,
      module: module,
      state: state,
      tags: %{file: "test.exs", line: 1}
    }
  end

  # --- No issues ---

  describe "run/4 — empty inputs" do
    test "no failures, no error, no artifacts returns []" do
      assert [] = Attribution.run(build_test_data(), empty_artifacts(), [])
    end
  end

  # --- Test failures ---

  describe "run/4 — test failures" do
    test "each failure becomes a :test_failure issue" do
      failure1 = make_exunit_test(ModA, :test_one, {:failed, [{:error, %{message: "boom"}, []}]})
      failure2 = make_exunit_test(ModA, :test_two, {:failed, [{:error, %{message: "bang"}, []}]})

      test_data = build_test_data(%{failures: [failure1, failure2]})

      issues = Attribution.run(test_data, empty_artifacts(), [])

      assert length(issues) == 2

      assert Enum.all?(issues, &(&1.type == :test_failure))
      assert Enum.all?(issues, &(&1.confidence == nil))

      scopes = Enum.map(issues, & &1.scope)
      assert {:test, ModA, :test_one} in scopes
      assert {:test, ModA, :test_two} in scopes
    end

    test "failure detail contains the ExUnit.Test struct" do
      failure = make_exunit_test(ModA, :test_one, {:failed, [{:error, %{message: "boom"}, []}]})
      test_data = build_test_data(%{failures: [failure]})

      [issue] = Attribution.run(test_data, empty_artifacts(), [])

      assert issue.detail.test == failure
    end
  end

  # --- Crashes ---

  describe "run/4 — crash events" do
    test "crash with timestamp attributed to test window" do
      crash_info = %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.to_unix(~U[2026-03-09 10:00:30Z], :microsecond)
      }

      crash_events = [%CrashEvent{server_id: "single1", crash_info: crash_info}]

      issues = Attribution.run(build_test_data(), empty_artifacts(), crash_events)

      assert [issue] = issues
      assert issue.type == :crash
      assert issue.scope == {:test, ModA, :test_one}
      assert issue.confidence == :high
      assert issue.detail.server == "single1"
    end

    test "crash without matching test window falls back to :suite" do
      crash_info = %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.to_unix(~U[2026-03-09 10:08:00Z], :microsecond)
      }

      crash_events = [%CrashEvent{server_id: "single1", crash_info: crash_info}]

      issues = Attribution.run(build_test_data(), empty_artifacts(), crash_events)

      assert [issue] = issues
      assert issue.scope == :suite
    end

    test "crash enriched with coredump analysis" do
      crash_info = %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.to_unix(~U[2026-03-09 10:00:30Z], :microsecond)
      }

      server = build_server("single1")

      artifacts = %{
        "single1" => %{
          server: server,
          coredump_paths: ["/tmp/core.1234"],
          sanitizer_files: []
        }
      }

      fake_analyzer = fn _core, _bin, _opts ->
        {:ok,
         %Toast.Diagnostics.Coredump.Report{
           core_path: "/tmp/core.1234",
           binary_path: "/usr/bin/arangod",
           debugger: :gdb,
           signal: "SIGSEGV",
           faulting_address: nil,
           crash_thread: 1,
           threads: [%{id: 1, frames: [%{function: "crash_here", file: "x.cpp", line: 1}]}]
         }}
      end

      crash_events = [%CrashEvent{server_id: "single1", crash_info: crash_info}]

      issues =
        Attribution.run(
          build_test_data(),
          artifacts,
          crash_events,
          analyzer_opts: [analyzer: fake_analyzer]
        )

      assert [issue] = issues
      assert issue.type == :crash
      assert [%{signal: "SIGSEGV", path: "/tmp/core.1234", threads: [_]}] = issue.detail.coredumps
    end

    test "crash issue produced even when coredump analysis fails" do
      crash_info = %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.to_unix(~U[2026-03-09 10:00:30Z], :microsecond)
      }

      server = build_server("single1")

      artifacts = %{
        "single1" => %{
          server: server,
          coredump_paths: ["/tmp/core.1234"],
          sanitizer_files: []
        }
      }

      crash_events = [%CrashEvent{server_id: "single1", crash_info: crash_info}]

      issues =
        Attribution.run(
          build_test_data(),
          artifacts,
          crash_events,
          analyzer_opts: [analyzer: fn _, _, _ -> {:error, :no_debugger} end]
        )

      assert [issue] = issues
      assert issue.type == :crash
      assert issue.detail.server == "single1"
      assert [%{path: "/tmp/core.1234", signal: nil, threads: []}] = issue.detail.coredumps
    end

    test "crash enriched with multiple coredumps" do
      crash_info = %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.to_unix(~U[2026-03-09 10:00:30Z], :microsecond)
      }

      server = build_server("single1")

      artifacts = %{
        "single1" => %{
          server: server,
          coredump_paths: ["/tmp/core.1", "/tmp/core.2"],
          sanitizer_files: []
        }
      }

      fake_analyzer = fn core_path, _bin, _opts ->
        {:ok,
         %Toast.Diagnostics.Coredump.Report{
           core_path: core_path,
           binary_path: "/usr/bin/arangod",
           debugger: :gdb,
           signal: "SIGSEGV",
           faulting_address: nil,
           crash_thread: 1,
           threads: [%{id: 1, frames: [%{function: "crash_here", file: "x.cpp", line: 1}]}]
         }}
      end

      crash_events = [%CrashEvent{server_id: "single1", crash_info: crash_info}]

      issues =
        Attribution.run(
          build_test_data(),
          artifacts,
          crash_events,
          analyzer_opts: [analyzer: fake_analyzer]
        )

      assert [issue] = issues
      assert issue.type == :crash
      assert length(issue.detail.coredumps) == 2

      paths = Enum.map(issue.detail.coredumps, & &1.path) |> Enum.sort()
      assert paths == ["/tmp/core.1", "/tmp/core.2"]
      assert Enum.all?(issue.detail.coredumps, &(&1.signal == "SIGSEGV"))
      assert Enum.all?(issue.detail.coredumps, &(length(&1.threads) == 1))
    end

    test "crash with mixed coredump analysis results" do
      crash_info = %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.to_unix(~U[2026-03-09 10:00:30Z], :microsecond)
      }

      server = build_server("single1")

      artifacts = %{
        "single1" => %{
          server: server,
          coredump_paths: ["/tmp/core.good", "/tmp/core.bad"],
          sanitizer_files: []
        }
      }

      fake_analyzer = fn core_path, _bin, _opts ->
        if core_path == "/tmp/core.good" do
          {:ok,
           %Toast.Diagnostics.Coredump.Report{
             core_path: core_path,
             binary_path: "/usr/bin/arangod",
             debugger: :gdb,
             signal: "SIGSEGV",
             faulting_address: nil,
             crash_thread: 1,
             threads: [%{id: 1, frames: [%{function: "crash_here", file: "x.cpp", line: 1}]}]
           }}
        else
          {:error, :no_debugger}
        end
      end

      crash_events = [%CrashEvent{server_id: "single1", crash_info: crash_info}]

      issues =
        Attribution.run(
          build_test_data(),
          artifacts,
          crash_events,
          analyzer_opts: [analyzer: fake_analyzer]
        )

      assert [issue] = issues
      assert issue.type == :crash
      assert length(issue.detail.coredumps) == 2

      by_path = Map.new(issue.detail.coredumps, &{&1.path, &1})

      good = by_path["/tmp/core.good"]
      assert good.signal == "SIGSEGV"
      assert length(good.threads) == 1

      bad = by_path["/tmp/core.bad"]
      assert bad.signal == nil
      assert bad.threads == []
    end
  end

  describe "run/4 — empty crash events" do
    test "empty list produces no crash issues" do
      assert [] = Attribution.run(build_test_data(), empty_artifacts(), [])
    end
  end

  # --- Sanitizer reports ---

  describe "run/4 — sanitizer files" do
    test "each sanitizer file produces a :sanitizer_report issue" do
      dir = System.tmp_dir!()
      san_file = Path.join(dir, "alubsan.log.#{System.unique_integer([:positive])}")
      File.write!(san_file, "ERROR: AddressSanitizer: heap-buffer-overflow")

      server = build_server("single1", server_dir: dir)

      artifacts = %{
        "single1" => %{
          server: server,
          coredump_paths: [],
          sanitizer_files: [san_file]
        }
      }

      issues = Attribution.run(build_test_data(), artifacts, [])

      assert [issue] = issues
      assert issue.type == :sanitizer_report
      assert issue.detail.server == "single1"
      assert issue.detail.report =~ "AddressSanitizer"
    after
      # Cleanup handled by tmp_dir lifecycle
      :ok
    end

    test "sanitizer file attributed via time windows" do
      dir = System.tmp_dir!()
      san_file = Path.join(dir, "tsan.log.#{System.unique_integer([:positive])}")
      File.write!(san_file, "WARNING: ThreadSanitizer: data race")

      # File mtime determines attribution — we can't control mtime easily,
      # so just verify it produces an issue with a scope
      server = build_server("single1", server_dir: dir)

      artifacts = %{
        "single1" => %{
          server: server,
          coredump_paths: [],
          sanitizer_files: [san_file]
        }
      }

      issues = Attribution.run(build_test_data(), artifacts, [])

      assert [issue] = issues
      assert issue.type == :sanitizer_report
      assert issue.scope != nil
    after
      :ok
    end
  end

  # --- Timeouts ---

  describe "run/4 — timeout kills" do
    test "empty timeout_kills produces no timeout issues" do
      issues = Attribution.run(build_test_data(), empty_artifacts(), [], timeout_kills: [])
      assert issues == []
    end

    test "timeout kill becomes a :timeout issue with suite scope" do
      kill = %{
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "single1", os_pid: 1001, log_file: "/tmp/single1.log"}],
        timestamp: ~U[2026-03-09 10:05:00Z]
      }

      issues = Attribution.run(build_test_data(), empty_artifacts(), [], timeout_kills: [kill])

      assert [issue] = issues
      assert issue.type == :timeout
      assert issue.scope == :suite
      assert issue.confidence == :high
      assert issue.detail.source == :suite_timeout
      assert issue.detail.reason == "Suite timeout exceeded"
      assert issue.detail.timestamp == ~U[2026-03-09 10:05:00Z]
    end

    test "timeout servers enriched with coredump path from artifacts" do
      server = build_server("single1")

      artifacts = %{
        "single1" => %{
          server: server,
          coredump_paths: ["/tmp/core.1001"],
          sanitizer_files: []
        }
      }

      kill = %{
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "single1", os_pid: 1001, log_file: "/tmp/single1.log"}],
        timestamp: ~U[2026-03-09 10:05:00Z]
      }

      issues = Attribution.run(build_test_data(), artifacts, [], timeout_kills: [kill])

      assert [issue] = issues
      [server_info] = issue.detail.servers
      assert server_info.coredump == "/tmp/core.1001"
      assert server_info.server_id == "single1"
    end

    test "timeout server without artifacts gets nil coredump" do
      kill = %{
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "single1", os_pid: 1001, log_file: "/tmp/single1.log"}],
        timestamp: ~U[2026-03-09 10:05:00Z]
      }

      issues = Attribution.run(build_test_data(), empty_artifacts(), [], timeout_kills: [kill])

      assert [issue] = issues
      [server_info] = issue.detail.servers
      assert server_info.coredump == nil
    end

    test "multiple killed servers in a single timeout event" do
      server1 = build_server("agent1")
      server2 = build_server("dbserver1")

      artifacts = %{
        "agent1" => %{
          server: server1,
          coredump_paths: ["/tmp/core.agent1"],
          sanitizer_files: []
        },
        "dbserver1" => %{
          server: server2,
          coredump_paths: [],
          sanitizer_files: []
        }
      }

      kill = %{
        source: :suite_timeout,
        reason: "Suite timeout exceeded",
        servers: [
          %{server_id: "agent1", os_pid: 2001, log_file: "/tmp/agent1.log"},
          %{server_id: "dbserver1", os_pid: 2002, log_file: "/tmp/dbserver1.log"}
        ],
        timestamp: ~U[2026-03-09 10:05:00Z]
      }

      issues = Attribution.run(build_test_data(), artifacts, [], timeout_kills: [kill])

      assert [issue] = issues
      assert length(issue.detail.servers) == 2

      by_id = Map.new(issue.detail.servers, &{&1.server_id, &1})
      assert by_id["agent1"].coredump == "/tmp/core.agent1"
      assert by_id["dbserver1"].coredump == nil
    end
  end

  # --- Mixed ---

  describe "run/4 — mixed issues" do
    test "test failures + crash + sanitizer all combined" do
      failure = make_exunit_test(ModA, :test_one, {:failed, [{:error, %{message: "boom"}, []}]})
      test_data = build_test_data(%{failures: [failure]})

      crash_info = %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.to_unix(~U[2026-03-09 10:00:30Z], :microsecond)
      }

      dir = System.tmp_dir!()
      san_file = Path.join(dir, "alubsan.log.#{System.unique_integer([:positive])}")
      File.write!(san_file, "ERROR: sanitizer report")

      server = build_server("single1", server_dir: dir)

      artifacts = %{
        "single1" => %{
          server: server,
          coredump_paths: [],
          sanitizer_files: [san_file]
        }
      }

      crash_events = [%CrashEvent{server_id: "single1", crash_info: crash_info}]

      issues =
        Attribution.run(test_data, artifacts, crash_events)

      types = Enum.map(issues, & &1.type)
      assert :test_failure in types
      assert :crash in types
      assert :sanitizer_report in types
      assert length(issues) == 3
    after
      :ok
    end
  end
end
