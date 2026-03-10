defmodule Toast.Deployment.DeploymentLifecycleTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.Controller

  import Toast.DeploymentTestHelpers, only: [make_deployment: 2]

  describe "status/1 with dead controller" do
    test "returns :stopped when controller process has died" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-status-dead")

      GenServer.stop(pid)

      assert Toast.Deployment.status(deployment) == :stopped
    end
  end
end
