defmodule ToastTest.StateCleanupTest do
  use ExUnit.Case, async: false

  alias ToastTest.{DeploymentRegistry, StateCleanup}

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

  test "reset clears deployment registry" do
    DeploymentRegistry.put(TestSuite, %{id: "dep-1"})
    assert DeploymentRegistry.get(TestSuite) == %{id: "dep-1"}

    StateCleanup.reset()

    assert_raise RuntimeError, fn -> DeploymentRegistry.get(TestSuite) end
  end

  test "reset clears abort table" do
    try do
      :ets.delete(:toast_suite_abort)
    catch
      :error, :badarg -> :ok
    end
    :ets.new(:toast_suite_abort, [:named_table, :set, :public])
    :ets.insert(:toast_suite_abort, {:aborted, "test reason"})

    StateCleanup.reset()

    assert :ets.lookup(:toast_suite_abort, :aborted) == []
  end

  test "reset does not crash when abort table does not exist" do
    try do
      :ets.delete(:toast_suite_abort)
    catch
      :error, :badarg -> :ok
    end

    StateCleanup.reset()
  end
end
