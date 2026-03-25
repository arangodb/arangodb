defmodule Toast.Deployment.DeploymentLifecycleTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.Controller

  import Toast.DeploymentTestHelpers, only: [make_deployment: 2]

  describe "status/1 with dead controller" do
    test "returns :stopped when controller process has died" do
      id = "test-status-dead-#{System.unique_integer([:positive])}"
      {:ok, pid} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      deployment = make_deployment(pid, id: id)

      GenServer.stop(pid)

      assert Toast.Deployment.status(deployment) == :stopped
    end
  end
end
