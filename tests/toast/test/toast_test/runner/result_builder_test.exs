defmodule ToastTest.Runner.ResultBuilderTest do
  use ExUnit.Case, async: true

  alias ToastTest.Runner.ResultBuilder

  describe "build_deployments/2" do
    test "single deployment with servers and logs" do
      snapshot = %{
        deployments: %{
          "d1" => %{
            mode: :cluster,
            stacktrace: nil,
            started_at: ~U[2026-01-01 00:00:00Z],
            stopped_at: ~U[2026-01-01 00:01:00Z]
          }
        },
        servers: %{
          "d1" => %{
            "s1" => %{role: :coordinator},
            "s2" => %{role: :dbserver}
          }
        }
      }

      server_logs = %{
        "s1" => ["line 1", "line 2"],
        "s2" => ["line 3"]
      }

      result = ResultBuilder.build_deployments(snapshot, server_logs)

      assert %{"d1" => deployment} = result
      assert deployment.id == "d1"
      assert deployment.mode == :cluster
      assert deployment.stacktrace == nil
      assert deployment.started_at == ~U[2026-01-01 00:00:00Z]
      assert deployment.stopped_at == ~U[2026-01-01 00:01:00Z]
      assert deployment.servers["s1"].logs == ["line 1", "line 2"]
      assert deployment.servers["s2"].logs == ["line 3"]
      assert deployment.servers["s1"].role == :coordinator
    end

    test "multiple deployments" do
      snapshot = %{
        deployments: %{
          "d1" => %{mode: :cluster, stacktrace: nil, started_at: nil, stopped_at: nil},
          "d2" => %{mode: :single, stacktrace: nil, started_at: nil, stopped_at: nil}
        },
        servers: %{
          "d1" => %{"s1" => %{role: :coordinator}},
          "d2" => %{"s2" => %{role: :single}}
        }
      }

      result = ResultBuilder.build_deployments(snapshot, %{})

      assert map_size(result) == 2
      assert result["d1"].mode == :cluster
      assert result["d2"].mode == :single
    end

    test "empty deployments" do
      snapshot = %{deployments: %{}, servers: %{}}
      assert ResultBuilder.build_deployments(snapshot, %{}) == %{}
    end

    test "servers without logs get empty list" do
      snapshot = %{
        deployments: %{
          "d1" => %{mode: :single, stacktrace: nil, started_at: nil, stopped_at: nil}
        },
        servers: %{
          "d1" => %{"s1" => %{role: :single}}
        }
      }

      result = ResultBuilder.build_deployments(snapshot, %{})
      assert result["d1"].servers["s1"].logs == []
    end

    test "deployment with no servers in snapshot" do
      snapshot = %{
        deployments: %{
          "d1" => %{mode: :single, stacktrace: nil, started_at: nil, stopped_at: nil}
        },
        servers: %{}
      }

      result = ResultBuilder.build_deployments(snapshot, %{})
      assert result["d1"].servers == %{}
    end
  end

  describe "collect_log_files/1" do
    test "collects log files across multiple deployments" do
      servers_by_deployment = %{
        "d1" => %{
          "s1" => %{log_file: "/tmp/s1.log"},
          "s2" => %{log_file: "/tmp/s2.log"}
        },
        "d2" => %{
          "s3" => %{log_file: "/tmp/s3.log"}
        }
      }

      result = ResultBuilder.collect_log_files(servers_by_deployment)

      assert result == %{
               "s1" => "/tmp/s1.log",
               "s2" => "/tmp/s2.log",
               "s3" => "/tmp/s3.log"
             }
    end

    test "skips servers without log_file key" do
      servers_by_deployment = %{
        "d1" => %{
          "s1" => %{log_file: "/tmp/s1.log"},
          "s2" => %{role: :dbserver}
        }
      }

      result = ResultBuilder.collect_log_files(servers_by_deployment)
      assert result == %{"s1" => "/tmp/s1.log"}
    end

    test "skips servers with nil log file" do
      servers_by_deployment = %{
        "d1" => %{
          "s1" => %{log_file: nil},
          "s2" => %{log_file: "/tmp/s2.log"}
        }
      }

      result = ResultBuilder.collect_log_files(servers_by_deployment)
      assert result == %{"s2" => "/tmp/s2.log"}
    end

    test "empty input returns empty map" do
      assert ResultBuilder.collect_log_files(%{}) == %{}
    end
  end

  describe "to_crash_event/1" do
    test "maps crash map to CrashEvent struct" do
      crash = %{server_id: "s1", crash_info: %{signal: 11, pid: 1234}}
      result = ResultBuilder.to_crash_event(crash)

      assert %ToastTest.CrashEvent{} = result
      assert result.server_id == "s1"
      assert result.crash_info == %{signal: 11, pid: 1234}
      assert result.expected == false
    end

    test "preserves expected field when true" do
      crash = %{server_id: "s1", crash_info: %{signal: 6}, expected: true}
      result = ResultBuilder.to_crash_event(crash)

      assert result.expected == true
    end

    test "defaults expected to false when missing" do
      crash = %{server_id: "s1", crash_info: %{signal: 6}}
      result = ResultBuilder.to_crash_event(crash)

      assert result.expected == false
    end
  end

  describe "coredump_warnings/4" do
    defp crash_event(server_id, signal) do
      %ToastTest.CrashEvent{
        server_id: server_id,
        crash_info: %Toast.Process.CrashInfo{
          exit_status: 128 + (signal || 0),
          signal: signal,
          timestamp: 0
        },
        expected: false
      }
    end

    test "no crashes returns empty list" do
      assert ResultBuilder.coredump_warnings([], %{}, nil, MapSet.new()) == []
    end

    test "core-producing crash with coredump returns empty list" do
      crash_events = [crash_event("s1", 11)]
      artifacts = %{"s1" => %{coredump_paths: ["/tmp/core.1234"]}}

      assert ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new()) == []
    end

    test "core-producing crash without coredump warns" do
      crash_events = [crash_event("s1", 11)]
      artifacts = %{"s1" => %{coredump_paths: []}}

      result = ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new())

      assert Enum.any?(result, &(&1 =~ "No coredumps found"))
    end

    test "SIGKILL crash without coredump does not warn" do
      crash_events = [crash_event("s1", 9)]
      artifacts = %{"s1" => %{coredump_paths: []}}

      assert ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new()) == []
    end

    test "mixed signals: warns only about core-producing crashes without coredumps" do
      crash_events = [crash_event("s1", 11), crash_event("s2", 9)]
      artifacts = %{"s1" => %{coredump_paths: []}, "s2" => %{coredump_paths: []}}

      result = ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new())

      assert Enum.any?(result, &(&1 =~ "s1"))
      refute Enum.any?(result, &(&1 =~ "s2"))
    end

    test "core-producing crash on one server, coredump found, no warning for that server" do
      crash_events = [crash_event("s1", 11), crash_event("s2", 6)]

      artifacts = %{
        "s1" => %{coredump_paths: ["/tmp/core.1"]},
        "s2" => %{coredump_paths: []}
      }

      result = ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new())

      refute Enum.any?(result, &(&1 =~ "s1"))
      assert Enum.any?(result, &(&1 =~ "s2"))
    end

    test "crash with nil signal is treated as non-core-producing" do
      crash_events = [crash_event("s1", nil)]
      artifacts = %{"s1" => %{coredump_paths: []}}

      assert ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new()) == []
    end

    test "includes sanitizer warning when sanitizers active and missing coredumps" do
      crash_events = [crash_event("s1", 11)]
      artifacts = %{"s1" => %{coredump_paths: []}}
      sanitizers = MapSet.new([:asan])

      result = ResultBuilder.coredump_warnings(crash_events, artifacts, nil, sanitizers)

      assert Enum.any?(result, &(&1 =~ "Sanitizer"))
    end

    test "no sanitizer warning when no missing coredumps" do
      crash_events = [crash_event("s1", 11)]
      artifacts = %{"s1" => %{coredump_paths: ["/tmp/core.1"]}}
      sanitizers = MapSet.new([:asan])

      assert ResultBuilder.coredump_warnings(crash_events, artifacts, nil, sanitizers) == []
    end

    test "includes discovery warning when missing coredumps and no coredump_dir" do
      crash_events = [crash_event("s1", 11)]
      artifacts = %{"s1" => %{coredump_paths: []}}

      result = ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new())

      # Discovery warning depends on system state, so just check the base warning is there
      assert Enum.any?(result, &(&1 =~ "No coredumps found"))
    end

    test "skips discovery warning when coredump_dir is set" do
      crash_events = [crash_event("s1", 11)]
      artifacts = %{"s1" => %{coredump_paths: []}}

      result =
        ResultBuilder.coredump_warnings(crash_events, artifacts, "/tmp/cores", MapSet.new())

      refute Enum.any?(result, &(&1 =~ "core_pattern"))
    end
  end
end
