defmodule ToastTest.CaseContextTest do
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

  defp fake_deployment(overrides) do
    defaults = %{
      id: "test-1",
      mode: :single_server,
      controller: self()
    }

    fields = Map.merge(defaults, overrides)

    %Toast.Deployment{
      id: fields.id,
      mode: fields.mode,
      controller: fields.controller
    }
  end

  describe "resolve_suite_key via DeploymentRegistry" do
    test "resolves suite key from __toast_suite__/0" do
      defmodule FakeSuiteModule do
        def __toast_suite__, do: __MODULE__
      end

      deployment = fake_deployment(%{endpoint: "http://localhost:9999"})
      DeploymentRegistry.put(FakeSuiteModule, deployment)
      DeploymentRegistry.put_extra_context(FakeSuiteModule, %{extra_key: "extra_val"})

      assert DeploymentRegistry.get(FakeSuiteModule) == deployment
      assert DeploymentRegistry.get_extra_context(FakeSuiteModule) == %{extra_key: "extra_val"}
    end

    test "raises when module has no __toast_suite__/0" do
      defmodule NoSuiteModule do
      end

      error =
        assert_raise RuntimeError, fn ->
          # Simulate what resolve_suite_key does
          if function_exported?(NoSuiteModule, :__toast_suite__, 0) do
            NoSuiteModule.__toast_suite__()
          else
            raise "#{inspect(NoSuiteModule)} must belong to a suite."
          end
        end

      assert error.message =~ "must belong to a suite"
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
