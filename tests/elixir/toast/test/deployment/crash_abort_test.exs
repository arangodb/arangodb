defmodule Toast.Deployment.CrashAbortTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.Controller
  alias Toast.Process.ServerProcess

  import Toast.DeploymentTestHelpers, only: [make_deployment: 1, make_deployment: 2]

  @fake_server Path.expand("../support/fake_server.sh", __DIR__)

  describe "crash status propagation" do
    test "controller status becomes :failed after crash message" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.utc_now()
      }

      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      assert Controller.get_status(pid) == :failed
      info = Controller.get_info(pid)
      assert {:server_crashed, "test-server", ^crash_info} = info.error
    end

    test "Deployment.status/1 returns :failed for crashed deployment" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 134,
        signal: 6,
        timestamp: DateTime.utc_now()
      }

      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      assert Toast.Deployment.status(deployment) == :failed
    end

    test "Deployment.status/1 returns :stopped when controller is dead" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-dead")
      GenServer.stop(pid)
      assert Toast.Deployment.status(deployment) == :stopped
    end

    test "Deployment.crash_info/1 returns :no_crash for healthy controller" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-healthy")
      assert Toast.Deployment.crash_info(deployment) == :no_crash
    end

    test "Deployment.crash_info/1 returns crash details" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.utc_now()
      }

      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      deployment = make_deployment(pid, id: "test-crash-info")
      assert {:ok, details} = Toast.Deployment.crash_info(deployment)
      assert details.server_id == "test-server"
      assert details.server_crash_info == crash_info
      assert details.log_report == nil
    end
  end

  describe "full crash chain with fake server" do
    test "ServerProcess crash propagates to Controller" do
      {:ok, controller_pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      {:ok, server_pid} =
        Toast.Process.Supervisor.start_server(
          id: "crash-chain-test",
          executable: @fake_server,
          args: ["--crash-after", "1"],
          listener: controller_pid
        )

      ServerProcess.launch(server_pid)

      assert poll_until(fn -> Controller.get_status(controller_pid) == :failed end),
             "Controller did not reach :failed status within timeout"

      info = Controller.get_info(controller_pid)
      assert {:server_crashed, "crash-chain-test", crash_info} = info.error
      assert crash_info.exit_status == 1
    end
  end

  describe "cluster crash propagation" do
    test "Controller (cluster) status becomes :failed after crash message" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.utc_now()
      }

      send(pid, {:server_crashed, "dbserver-1", crash_info})
      :sys.get_state(pid)

      assert Controller.get_status(pid) == :failed
      info = Controller.get_info(pid)
      assert {:server_crashed, "dbserver-1", ^crash_info} = info.error
    end

    test "Deployment.status/1 returns :failed for cluster deployment with crashed server" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-cluster", mode: :cluster)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.utc_now()
      }

      send(pid, {:server_crashed, "agent-1", crash_info})
      :sys.get_state(pid)

      assert Toast.Deployment.status(deployment) == :failed
    end

    test "Deployment.crash_info/1 returns cluster crash details with server_id" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-cluster-crash", mode: :cluster)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.utc_now()
      }

      send(pid, {:server_crashed, "agent-1", crash_info})
      :sys.get_state(pid)

      assert {:ok, details} = Toast.Deployment.crash_info(deployment)
      assert details.server_id == "agent-1"
      assert details.server_crash_info == crash_info
    end

    test "Deployment.crash_info/1 returns :no_crash for healthy cluster controller" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-cluster-healthy", mode: :cluster)
      assert Toast.Deployment.crash_info(deployment) == :no_crash
    end
  end

  describe "on_crash callback" do
    test "controller invokes on_crash callback on unexpected crash" do
      test_pid = self()
      on_crash = fn _server_id, crash_info -> send(test_pid, {:crash_callback, crash_info}) end

      {:ok, pid} =
        Controller.start_link(
          mode: Controller.SingleServer,
          config: Toast.Config.load(),
          on_crash: on_crash
        )

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.utc_now()
      }

      send(pid, {:server_crashed, "test-server", crash_info})

      assert_receive {:crash_callback, _crash_info}, 1_000
    end
  end

  describe "on_event callback" do
    test "on_event fires for :server_crashed" do
      test_pid = self()
      on_event = fn event -> send(test_pid, {:event, event}) end

      {:ok, pid} =
        Controller.start_link(
          mode: Controller.SingleServer,
          config: Toast.Config.load(),
          on_event: on_event
        )

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: DateTime.utc_now()
      }

      send(pid, {:server_crashed, "test-server", crash_info})

      assert_receive {:event, {:server_crashed, "test-server", _, _, _}}, 1_000
    end
  end

  describe "ToastTest.Runner.abort!" do
    setup do
      ToastTest.Runner.clear_abort!()
      on_exit(fn -> ToastTest.Runner.clear_abort!() end)
    end

    test "sets abort state" do
      assert ToastTest.Runner.aborted?() == nil
      ToastTest.Runner.abort!("Server crashed: test-srv")
      assert ToastTest.Runner.aborted?() == "Server crashed: test-srv"
    end

    test "clear_abort! resets state" do
      ToastTest.Runner.abort!("reason")
      ToastTest.Runner.clear_abort!()
      assert ToastTest.Runner.aborted?() == nil
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
