defmodule Toast.Deployment.CrashAbortTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.{Controller, ClusterController}
  alias Toast.Process.ServerProcess

  @fake_server Path.expand("../support/fake_server.sh", __DIR__)

  describe "crash status propagation" do
    test "controller status becomes :failed after crash message" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})
      # Allow message processing
      :sys.get_state(pid)

      assert Controller.get_status(pid) == :failed
      info = Controller.get_info(pid)
      assert {:server_crashed, ^crash_info} = info.error
    end

    test "Deployment.status/1 returns :failed for crashed deployment" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-crash",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      crash_info = %{exit_status: 134, signal: 6, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      assert Toast.Deployment.status(deployment) == :failed
    end

    test "Deployment.status/1 returns :stopped when controller is dead" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-dead",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      GenServer.stop(pid)

      assert Toast.Deployment.status(deployment) == :stopped
    end

    test "Deployment.crash_info/1 returns :no_crash for healthy controller" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-healthy",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      assert Toast.Deployment.crash_info(deployment) == :no_crash
    end

    test "Deployment.crash_info/1 returns crash details" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      deployment = %Toast.Deployment{
        id: "test-crash-info",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      assert {:ok, details} = Toast.Deployment.crash_info(deployment)
      assert details.server_id == nil
      assert details.server_crash_info == crash_info
      # log_report will be nil since there's no actual log file
      assert details.log_report == nil
    end
  end

  describe "full crash chain with fake server" do
    test "ServerProcess crash propagates to Controller" do
      {:ok, controller_pid} = Controller.start_link(config: Toast.Config.load())

      {:ok, server_pid} =
        Toast.Process.Supervisor.start_server(
          id: "crash-chain-test",
          executable: @fake_server,
          args: ["--crash-after", "1"],
          listener: controller_pid
        )

      ServerProcess.launch(server_pid)

      # Poll until the crash propagates to the controller
      assert poll_until(fn -> Controller.get_status(controller_pid) == :failed end),
             "Controller did not reach :failed status within timeout"
      info = Controller.get_info(controller_pid)
      assert {:server_crashed, crash_info} = info.error
      assert crash_info.exit_status == 1
    end
  end

  describe "cluster crash propagation" do
    test "ClusterController status becomes :failed after crash message" do
      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load())

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "dbserver-1", crash_info})
      :sys.get_state(pid)

      assert ClusterController.get_status(pid) == :failed
      info = ClusterController.get_info(pid)
      assert {:server_crashed, "dbserver-1", ^crash_info} = info.error
    end

    test "Deployment.status/1 returns :failed for cluster deployment with crashed server" do
      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-cluster",
        mode: :cluster,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "agent-1", crash_info})
      :sys.get_state(pid)

      assert Toast.Deployment.status(deployment) == :failed
    end

    test "Deployment.crash_info/1 returns cluster crash details with server_id" do
      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-cluster-crash",
        mode: :cluster,
        endpoint: "http://127.0.0.1:0",
        controller: pid,
        servers: %{
          "agent-1" => %{
            role: :agent,
            port: 8531,
            endpoint: "http://127.0.0.1:8531",
            log_file: nil
          }
        }
      }

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "agent-1", crash_info})
      :sys.get_state(pid)

      assert {:ok, details} = Toast.Deployment.crash_info(deployment)
      assert details.server_id == "agent-1"
      assert details.server_crash_info == crash_info
    end

    test "Deployment.crash_info/1 returns :no_crash for healthy cluster controller" do
      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-cluster-healthy",
        mode: :cluster,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      assert Toast.Deployment.crash_info(deployment) == :no_crash
    end
  end

  defp poll_until(condition, timeout_ms \\ 5_000, interval_ms \\ 50) do
    deadline = System.monotonic_time(:millisecond) + timeout_ms
    do_poll_until(condition, deadline, interval_ms)
  end

  defp do_poll_until(condition, deadline, interval_ms) do
    if condition.() do
      true
    else
      if System.monotonic_time(:millisecond) >= deadline do
        false
      else
        Process.sleep(interval_ms)
        do_poll_until(condition, deadline, interval_ms)
      end
    end
  end
end
