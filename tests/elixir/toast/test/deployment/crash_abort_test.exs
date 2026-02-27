defmodule Toast.Deployment.CrashAbortTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.{SingleServerController, ClusterController}
  alias Toast.Process.ServerProcess

  @fake_server Path.expand("../support/fake_server.sh", __DIR__)

  defp make_deployment(pid, opts \\ []) do
    %Toast.Deployment{
      id: Keyword.get(opts, :id, "test-crash"),
      mode: Keyword.get(opts, :mode, :single_server),
      config: Toast.Config.load(),
      endpoint: "http://127.0.0.1:0",
      controller: pid,
      work_dir: "/tmp/toast-test"
    }
  end

  describe "crash status propagation" do
    test "controller status becomes :failed after crash message" do
      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load())

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      assert SingleServerController.get_status(pid) == :failed
      info = SingleServerController.get_info(pid)
      assert {:server_crashed, ^crash_info} = info.error
    end

    test "Deployment.status/1 returns :failed for crashed deployment" do
      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid)

      crash_info = %{exit_status: 134, signal: 6, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      assert Toast.Deployment.status(deployment) == :failed
    end

    test "Deployment.status/1 returns :stopped when controller is dead" do
      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-dead")
      GenServer.stop(pid)
      assert Toast.Deployment.status(deployment) == :stopped
    end

    test "Deployment.crash_info/1 returns :no_crash for healthy controller" do
      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-healthy")
      assert Toast.Deployment.crash_info(deployment) == :no_crash
    end

    test "Deployment.crash_info/1 returns crash details" do
      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load())
      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      deployment = make_deployment(pid, id: "test-crash-info")
      assert {:ok, details} = Toast.Deployment.crash_info(deployment)
      assert details.server_id == nil
      assert details.server_crash_info == crash_info
      assert details.log_report == nil
    end
  end

  describe "full crash chain with fake server" do
    test "ServerProcess crash propagates to Controller" do
      {:ok, controller_pid} = SingleServerController.start_link(config: Toast.Config.load())

      {:ok, server_pid} =
        Toast.Process.Supervisor.start_server(
          id: "crash-chain-test",
          executable: @fake_server,
          args: ["--crash-after", "1"],
          listener: controller_pid
        )

      ServerProcess.launch(server_pid)

      assert poll_until(fn -> SingleServerController.get_status(controller_pid) == :failed end),
             "Controller did not reach :failed status within timeout"

      info = SingleServerController.get_info(controller_pid)
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
      deployment = make_deployment(pid, id: "test-cluster", mode: :cluster)

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "agent-1", crash_info})
      :sys.get_state(pid)

      assert Toast.Deployment.status(deployment) == :failed
    end

    test "Deployment.crash_info/1 returns cluster crash details with server_id" do
      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-cluster-crash", mode: :cluster)

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "agent-1", crash_info})
      :sys.get_state(pid)

      assert {:ok, details} = Toast.Deployment.crash_info(deployment)
      assert details.server_id == "agent-1"
      assert details.server_crash_info == crash_info
    end

    test "Deployment.crash_info/1 returns :no_crash for healthy cluster controller" do
      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-cluster-healthy", mode: :cluster)
      assert Toast.Deployment.crash_info(deployment) == :no_crash
    end
  end

  describe "on_crash callback" do
    test "controller invokes on_crash callback on unexpected crash" do
      test_pid = self()
      on_crash = fn _deployment, crash_info -> send(test_pid, {:crash_callback, crash_info}) end

      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load(), on_crash: on_crash)

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})

      assert_receive {:crash_callback, ^crash_info}, 1_000
    end

    test "cluster controller invokes on_crash callback on unexpected crash" do
      test_pid = self()
      on_crash = fn _deployment, crash_info -> send(test_pid, {:crash_callback, crash_info}) end

      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load(), on_crash: on_crash)

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "dbserver-1", crash_info})

      assert_receive {:crash_callback, ^crash_info}, 1_000
    end

    test "no crash callback when none provided" do
      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load())

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      assert SingleServerController.get_status(pid) == :failed
    end
  end

  describe "on_event callback" do
    test "on_event fires for :server_crashed" do
      test_pid = self()
      on_event = fn event -> send(test_pid, {:event, event}) end

      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load(), on_event: on_event)

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})

      assert_receive {:event, {:server_crashed, "test-server", _, ^crash_info, _}}, 1_000
    end

    test "no event callback when none provided" do
      {:ok, pid} = SingleServerController.start_link(config: Toast.Config.load())

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(pid, {:server_crashed, "test-server", crash_info})
      :sys.get_state(pid)

      assert SingleServerController.get_status(pid) == :failed
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
