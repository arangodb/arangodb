defmodule Toast.Deployment.DeploymentLifecycleTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.Controller

  describe "stop_and_collect/2" do
    test "returns nil for a controller that never deployed" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-collect-never-deployed",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      # Controller is :stopped (never deployed), shutdown on :stopped returns :ok,
      # but diagnostics will be nil since server_dir is nil.
      result = Toast.Deployment.stop_and_collect(deployment)
      assert result == nil
    end

    test "returns nil for a controller that was already shut down" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-collect-already-stopped",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      # Shutdown the controller first
      :ok = Controller.shutdown(pid)

      # Calling stop_and_collect again should still return nil (already stopped, no diagnostics)
      result = Toast.Deployment.stop_and_collect(deployment)
      assert result == nil
    end

    test "exits when controller process is dead" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-collect-dead",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      GenServer.stop(pid)

      # stop_and_collect does not use controller_call (which has try/catch),
      # so calling it on a dead process raises an exit.
      assert catch_exit(Toast.Deployment.stop_and_collect(deployment)) != nil
    end
  end

  describe "diagnostics/1" do
    test "returns nil for a controller that never deployed" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-diagnostics-clean",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      assert Toast.Deployment.diagnostics(deployment) == nil
    end

    test "returns nil when controller process is dead" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-diagnostics-dead",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      GenServer.stop(pid)

      # The try/catch in controller_call catches :exit and returns nil
      assert Toast.Deployment.diagnostics(deployment) == nil
    end
  end

  describe "status/1 with dead controller" do
    test "returns :stopped when controller process has died" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      deployment = %Toast.Deployment{
        id: "test-status-dead",
        mode: :single_server,
        endpoint: "http://127.0.0.1:0",
        controller: pid
      }

      GenServer.stop(pid)

      assert Toast.Deployment.status(deployment) == :stopped
    end
  end
end
