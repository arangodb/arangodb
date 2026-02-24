defmodule Toast.Deployment.DeploymentLifecycleTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.SingleServerController, as: Controller

  defp make_deployment(pid, opts \\ []) do
    %Toast.Deployment{
      id: Keyword.get(opts, :id, "test-lifecycle"),
      mode: :single_server,
      config: Toast.Config.load(),
      endpoint: "http://127.0.0.1:0",
      controller: pid,
      work_dir: "/tmp/toast-test"
    }
  end

  describe "stop_and_collect/2" do
    test "returns nil for a controller that never deployed" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-collect-never-deployed")

      result = Toast.Deployment.stop_and_collect(deployment)
      assert result == nil
    end

    test "returns nil for a controller that was already shut down" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-collect-already-stopped")

      :ok = Controller.shutdown(pid)

      result = Toast.Deployment.stop_and_collect(deployment)
      assert result == nil
    end

    test "exits when controller process is dead" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-collect-dead")

      GenServer.stop(pid)

      assert catch_exit(Toast.Deployment.stop_and_collect(deployment)) != nil
    end
  end

  describe "diagnostics/1" do
    test "returns nil for a controller that never deployed" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-diagnostics-clean")

      assert Toast.Deployment.diagnostics(deployment) == nil
    end

    test "returns nil when controller process is dead" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-diagnostics-dead")

      GenServer.stop(pid)

      assert Toast.Deployment.diagnostics(deployment) == nil
    end
  end

  describe "status/1 with dead controller" do
    test "returns :stopped when controller process has died" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-status-dead")

      GenServer.stop(pid)

      assert Toast.Deployment.status(deployment) == :stopped
    end
  end
end
