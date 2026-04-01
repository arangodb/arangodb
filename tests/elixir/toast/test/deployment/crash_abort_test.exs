defmodule Toast.Deployment.CrashAbortTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.{Controller, ServerInstance}
  alias Toast.Process.ServerProcess

  import Toast.DeploymentTestHelpers, only: [make_deployment: 1, make_deployment: 2]

  @fake_server Path.expand("../support/fake_server.sh", __DIR__)

  describe "crash status propagation" do
    test "controller status becomes :failed after crash message" do
      id = "crash-prop-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      # Inject a server so the crash handler can find it
      :sys.replace_state(pid, fn state ->
        %{state | servers: %{"test-server" => %ServerInstance{id: "test-server", role: :single}}}
      end)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: :os.system_time(:microsecond)
      }

      send(pid, {:server_crashed, "test-server", crash_info})

      assert Controller.get_status(pid) == :failed
      info = Controller.get_info(pid)
      assert {:server_crashed, "test-server", ^crash_info} = info.error
    end

    test "Deployment.status/1 returns :failed for crashed deployment" do
      id = "crash-deploy-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      deployment = make_deployment(pid)

      :sys.replace_state(pid, fn state ->
        %{state | servers: %{"test-server" => %ServerInstance{id: "test-server", role: :single}}}
      end)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 134,
        signal: 6,
        timestamp: :os.system_time(:microsecond)
      }

      send(pid, {:server_crashed, "test-server", crash_info})

      assert Toast.Deployment.status(deployment) == :failed
    end

    test "Deployment.status/1 returns :stopped when controller is dead" do
      id = "crash-dead-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      deployment = make_deployment(pid, id: id)
      GenServer.stop(pid)
      assert Toast.Deployment.status(deployment) == :stopped
    end

    test "Deployment.deployment_error/1 returns nil for healthy controller" do
      id = "crash-healthy-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      deployment = make_deployment(pid, id: id)
      assert Toast.Deployment.deployment_error(deployment) == nil
    end

    test "Deployment.deployment_error/1 returns crash details" do
      id = "crash-details-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      :sys.replace_state(pid, fn state ->
        %{state | servers: %{"test-server" => %ServerInstance{id: "test-server", role: :single}}}
      end)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: :os.system_time(:microsecond)
      }

      send(pid, {:server_crashed, "test-server", crash_info})

      deployment = make_deployment(pid, id: id)

      assert {:server_crashed, "test-server", ^crash_info} =
               Toast.Deployment.deployment_error(deployment)
    end
  end

  describe "full crash chain with fake server" do
    test "ServerProcess crash propagates to Controller" do
      id = "crash-chain-#{System.unique_integer([:positive])}"

      {:ok, controller_pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      :sys.replace_state(controller_pid, fn state ->
        %{
          state
          | servers: %{
              "crash-chain-test" => %ServerInstance{id: "crash-chain-test", role: :single}
            }
        }
      end)

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
      id = "cluster-crash-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      :sys.replace_state(pid, fn state ->
        %{state | servers: %{"dbserver-1" => %ServerInstance{id: "dbserver-1", role: :dbserver}}}
      end)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: :os.system_time(:microsecond)
      }

      send(pid, {:server_crashed, "dbserver-1", crash_info})

      assert Controller.get_status(pid) == :failed
      info = Controller.get_info(pid)
      assert {:server_crashed, "dbserver-1", ^crash_info} = info.error
    end

    test "Deployment.status/1 returns :failed for cluster deployment with crashed server" do
      id = "cluster-crash-deploy-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      deployment = make_deployment(pid, id: id)

      :sys.replace_state(pid, fn state ->
        %{state | servers: %{"agent-1" => %ServerInstance{id: "agent-1", role: :agent}}}
      end)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: :os.system_time(:microsecond)
      }

      send(pid, {:server_crashed, "agent-1", crash_info})

      assert Toast.Deployment.status(deployment) == :failed
    end

    test "Deployment.deployment_error/1 returns cluster crash details with server_id" do
      id = "cluster-crash-error-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      deployment = make_deployment(pid, id: id)

      :sys.replace_state(pid, fn state ->
        %{state | servers: %{"agent-1" => %ServerInstance{id: "agent-1", role: :agent}}}
      end)

      crash_info = %Toast.Process.CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: :os.system_time(:microsecond)
      }

      send(pid, {:server_crashed, "agent-1", crash_info})

      assert {:server_crashed, "agent-1", ^crash_info} =
               Toast.Deployment.deployment_error(deployment)
    end

    test "Deployment.deployment_error/1 returns nil for healthy cluster controller" do
      id = "cluster-healthy-#{System.unique_integer([:positive])}"

      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      deployment = make_deployment(pid, id: id)
      assert Toast.Deployment.deployment_error(deployment) == nil
    end
  end

  describe "ToastTest.Abort" do
    setup do
      ToastTest.Abort.clear!()
      on_exit(fn -> ToastTest.Abort.clear!() end)
    end

    test "sets abort state" do
      assert ToastTest.Abort.reason() == nil
      ToastTest.Abort.abort!("Server crashed: test-srv")
      assert ToastTest.Abort.reason() == "Server crashed: test-srv"
    end

    test "clear! resets state" do
      ToastTest.Abort.abort!("reason")
      ToastTest.Abort.clear!()
      assert ToastTest.Abort.reason() == nil
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
