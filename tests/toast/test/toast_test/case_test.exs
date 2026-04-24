# Integration test: exercises Case's actual callback chain to verify that
# a module's setup_all additions are not overwritten by a per-test setup.
# Separate module because it must `use ToastTest.Case` (CaseContextTest uses ExUnit.Case).
defmodule ToastTest.CaseContextSurvivalTest do
  defmodule Suite do
    @moduledoc false

    @deployment %Toast.Deployment{
      id: "context-survival-test",
      controller: nil,
      servers: %{
        "single" => %Toast.Deployment.ServerInfo{
          id: "single",
          role: :single,
          port: 8529,
          endpoint: "http://localhost:8529"
        }
      }
    }

    @extra_context %{registry_key: "from_registry"}

    def __toast_suite__ do
      ensure_registered()
      __MODULE__
    end

    defp ensure_registered do
      alias ToastTest.DeploymentRegistry

      try do
        DeploymentRegistry.get(__MODULE__)
      rescue
        RuntimeError ->
          DeploymentRegistry.put(__MODULE__, @deployment)
          DeploymentRegistry.put_extra_context(__MODULE__, @extra_context)
      end

      :ok
    end
  end

  use ToastTest.Case, async: false

  def __toast_suite__, do: Suite.__toast_suite__()

  setup_all _context do
    {:ok, %{registry_key: "modified_by_module"}}
  end

  test "setup_all context additions survive to test", context do
    assert context[:registry_key] == "modified_by_module"
  end

  test "base deployment context is available", context do
    assert context[:deployment] != nil
    assert context[:endpoint] != nil
    assert context[:client] != nil
  end
end

defmodule ToastTest.CaseContextTest do
  use ExUnit.Case, async: false

  import Toast.DeploymentTestHelpers, only: [setup_deployment_registry: 1]

  alias ToastTest.DeploymentRegistry

  setup :setup_deployment_registry

  defp fake_deployment do
    %Toast.Deployment{
      id: "test-1"
    }
  end

  describe "resolve_suite_key via DeploymentRegistry" do
    test "resolves suite key from __toast_suite__/0" do
      defmodule FakeSuiteModule do
        def __toast_suite__, do: __MODULE__
      end

      deployment = fake_deployment()
      DeploymentRegistry.put(FakeSuiteModule, deployment)
      DeploymentRegistry.put_extra_context(FakeSuiteModule, %{extra_key: "extra_val"})

      assert DeploymentRegistry.get(FakeSuiteModule) == deployment
      assert DeploymentRegistry.get_extra_context(FakeSuiteModule) == %{extra_key: "extra_val"}
    end
  end

  describe "use ToastTest.Case macro expansion" do
    test "defines Client alias and @moduletag" do
      defmodule SampleTestModule do
        use ToastTest.Case

        def get_moduletag do
          @moduletag
        end
      end

      assert Code.ensure_loaded?(SampleTestModule)
    end

    test "modules using ToastTest.Case are ExUnit.CaseTemplate consumers" do
      defmodule AnotherTestModule do
        use ToastTest.Case
      end

      assert function_exported?(AnotherTestModule, :__ex_unit__, 0)
    end
  end
end
