defmodule Toast.DeploymentTestHelpers do
  @moduledoc false

  def setup_deployment_registry(_context) do
    try do
      :ets.delete(:toast_deployment_registry)
    catch
      :error, :badarg -> :ok
    end

    ToastTest.DeploymentRegistry.init()

    ExUnit.Callbacks.on_exit(fn ->
      try do
        :ets.delete(:toast_deployment_registry)
      catch
        :error, :badarg -> :ok
      end
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
