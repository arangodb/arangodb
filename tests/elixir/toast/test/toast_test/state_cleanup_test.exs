defmodule ToastTest.StateCleanupTest do
  use ExUnit.Case, async: false

  import Toast.DeploymentTestHelpers, only: [setup_deployment_registry: 1]

  alias ToastTest.{DeploymentRegistry, StateCleanup}

  setup :setup_deployment_registry

  setup do
    original_after_suite = Application.get_env(:ex_unit, :after_suite, [])

    on_exit(fn ->
      Application.put_env(:ex_unit, :after_suite, original_after_suite)
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

  test "reset clears after_suite callbacks" do
    Application.put_env(:ex_unit, :after_suite, [fn _ -> :ok end])

    StateCleanup.reset()

    assert Application.get_env(:ex_unit, :after_suite) == []
  end

  test "reset does not touch port allocator state" do
    try do
      :ets.delete(:toast_port_allocator)
    catch
      :error, :badarg -> :ok
    end

    :ets.new(:toast_port_allocator, [:named_table, :set, :public])
    :ets.insert(:toast_port_allocator, {:next_port, 8530})

    StateCleanup.reset()

    assert :ets.lookup(:toast_port_allocator, :next_port) == [{:next_port, 8530}]
  after
    try do
      :ets.delete(:toast_port_allocator)
    catch
      :error, :badarg -> :ok
    end
  end
end
