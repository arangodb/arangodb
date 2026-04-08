defmodule Toast.DeploymentTestHelpers do
  @moduledoc false

  def setup_deployment_registry(_context) do
    ToastTest.DeploymentRegistry.clear()

    ExUnit.Callbacks.on_exit(fn ->
      ToastTest.DeploymentRegistry.clear()
    end)

    :ok
  end

  def make_deployment(pid, opts \\ []) do
    %Toast.Deployment{
      id: Keyword.get(opts, :id, "test-deployment"),
      controller: pid
    }
  end
end
