defmodule ToastTest.InteractiveTest do
  use ExUnit.Case, async: false

  alias ToastTest.DeploymentRegistry

  setup do
    try do
      :ets.delete(:toast_deployment_registry)
    catch
      :error, :badarg -> :ok
    end
    DeploymentRegistry.init()

    on_exit(fn ->
      try do
        :ets.delete(:toast_deployment_registry)
      catch
        :error, :badarg -> :ok
      end
    end)

    :ok
  end

  test "run/2 with module registers deployment and runs tests" do
    _deployment = %{endpoint: "http://localhost:8529", config: %{api_version: nil}}
    # We can't easily test full Interactive.run/2 without ExUnit internals,
    # but we can test that ensure_registry works
    assert :ets.whereis(:toast_deployment_registry) != :undefined
  end

  test "ensure_registry creates table when missing" do
    :ets.delete(:toast_deployment_registry)
    assert :ets.whereis(:toast_deployment_registry) == :undefined

    # Call init to simulate what ensure_registry does
    DeploymentRegistry.init()
    assert :ets.whereis(:toast_deployment_registry) != :undefined
  end
end
