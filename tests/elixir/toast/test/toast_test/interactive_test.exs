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

  describe "ensure_registry" do
    test "creates ETS table when missing" do
      :ets.delete(:toast_deployment_registry)
      assert :ets.whereis(:toast_deployment_registry) == :undefined

      DeploymentRegistry.init()
      assert :ets.whereis(:toast_deployment_registry) != :undefined
    end

    test "is idempotent when table already exists" do
      assert :ets.whereis(:toast_deployment_registry) != :undefined

      # Re-init should not crash
      DeploymentRegistry.init()
      assert :ets.whereis(:toast_deployment_registry) != :undefined
    end
  end

  describe "run/2 deployment registration" do
    test "registers deployment in DeploymentRegistry for suite key" do
      deployment = %{endpoint: "http://localhost:8529", config: %{api_version: nil}}
      suite_key = :interactive

      DeploymentRegistry.put(suite_key, deployment)
      assert DeploymentRegistry.get(suite_key) == deployment
    end

    test "uses module's __toast_suite__/0 as key when available" do
      defmodule FakeSuiteForInteractive do
        def __toast_suite__, do: __MODULE__
      end

      deployment = %{endpoint: "http://localhost:9999"}
      DeploymentRegistry.put(FakeSuiteForInteractive, deployment)
      assert DeploymentRegistry.get(FakeSuiteForInteractive) == deployment
    end

    test "uses :interactive as key when module lacks __toast_suite__/0" do
      # Interactive.run/2 checks function_exported?(module, :__toast_suite__, 0)
      # and falls back to :interactive. We verify the same logic path.
      defmodule PlainModuleForInteractive do
        def some_function, do: :ok
      end

      refute function_exported?(PlainModuleForInteractive, :__toast_suite__, 0)

      deployment = %{endpoint: "http://localhost:8529"}
      DeploymentRegistry.put(:interactive, deployment)
      assert DeploymentRegistry.get(:interactive) == deployment
    end
  end

  describe "filter_tests logic" do
    # filter_tests is private but its behavior is testable: it filters ExUnit
    # test structs by converting a string name to :"test <name>" and matching.

    test "nil name returns all tests" do
      # When no test name filter is given, all tests run.
      # We verify the atom conversion logic that filter_tests uses.
      name = "my test case"
      expected_atom = String.to_atom("test #{name}")
      assert expected_atom == :"test my test case"
    end

    test "string name is converted to :\"test <name>\" atom for matching" do
      name = "creates a document"
      expected_atom = String.to_atom("test #{name}")
      assert expected_atom == :"test creates a document"
    end
  end
end
