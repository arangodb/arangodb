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

      assert %Toast.Process.CrashEvent{} = result
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
    test "no crashes returns empty list" do
      assert ResultBuilder.coredump_warnings([], %{}, nil, MapSet.new()) == []
    end

    test "crashes with coredumps returns empty list" do
      crash_events = [
        %Toast.Process.CrashEvent{server_id: "s1", crash_info: %{}, expected: false}
      ]

      artifacts = %{"s1" => %{coredump_paths: ["/tmp/core.1234"]}}

      assert ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new()) == []
    end

    test "crashes without coredumps and no sanitizers produces no sanitizer warning" do
      crash_events = [
        %Toast.Process.CrashEvent{server_id: "s1", crash_info: %{}, expected: false}
      ]

      artifacts = %{"s1" => %{coredump_paths: []}}

      result = ResultBuilder.coredump_warnings(crash_events, artifacts, nil, MapSet.new())

      refute Enum.any?(result, &String.contains?(&1, "Sanitizer"))
    end

    test "crashes without coredumps and active sanitizers includes sanitizer warning" do
      crash_events = [
        %Toast.Process.CrashEvent{server_id: "s1", crash_info: %{}, expected: false}
      ]

      artifacts = %{"s1" => %{coredump_paths: []}}
      sanitizers = MapSet.new([:asan])

      result = ResultBuilder.coredump_warnings(crash_events, artifacts, nil, sanitizers)

      assert Enum.any?(result, &String.contains?(&1, "Sanitizer"))
    end

    test "crashes without coredumps with coredump_dir skips discovery warning" do
      crash_events = [
        %Toast.Process.CrashEvent{server_id: "s1", crash_info: %{}, expected: false}
      ]

      artifacts = %{"s1" => %{coredump_paths: []}}
      sanitizers = MapSet.new([:asan])

      # coredump_discovery_warning returns nil for non-nil dir, compact filters it
      result = ResultBuilder.coredump_warnings(crash_events, artifacts, "/tmp/cores", sanitizers)

      assert length(result) == 1
      assert hd(result) =~ "Sanitizer"
    end
  end
end
