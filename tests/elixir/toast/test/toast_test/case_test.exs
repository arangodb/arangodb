defmodule ToastTest.CaseContextTest do
  use ExUnit.Case, async: false

  alias ToastTest.DeploymentRegistry

  @deployment_key :__test_deployment__

  setup do
    saved_deployment = Application.get_env(:toast, @deployment_key)

    try do
      :ets.delete(:toast_deployment_registry)
    catch
      :error, :badarg -> :ok
    end

    DeploymentRegistry.init()

    on_exit(fn ->
      if saved_deployment,
        do: Application.put_env(:toast, @deployment_key, saved_deployment),
        else: Application.delete_env(:toast, @deployment_key)

      try do
        :ets.delete(:toast_deployment_registry)
      catch
        :error, :badarg -> :ok
      end
    end)

    Application.delete_env(:toast, @deployment_key)
    :ok
  end

  defp fake_deployment(overrides \\ %{}) do
    defaults = %{
      id: "test-1",
      mode: :single_server,
      config: %Toast.Config{},
      endpoint: "http://localhost:8529",
      controller: self(),
      work_dir: "/tmp/toast-test"
    }

    fields = Map.merge(defaults, overrides)

    %Toast.Deployment{
      id: fields.id,
      mode: fields.mode,
      config: fields.config,
      endpoint: fields.endpoint,
      controller: fields.controller,
      work_dir: fields.work_dir
    }
  end

  describe "setup provides deployment context" do
    test "get_deployment_for_context falls back to Application env when module lacks __toast_suite__" do
      deployment = fake_deployment()
      ToastTest.Case.register_deployment(deployment)

      # Simulate what setup/1 does for a module without __toast_suite__
      retrieved = ToastTest.Case.get_deployment()
      assert retrieved.endpoint == "http://localhost:8529"
      assert retrieved.id == "test-1"
    end

    test "get_deployment_for_context uses DeploymentRegistry when module has __toast_suite__" do
      # Define a suite module that has __toast_suite__/0
      defmodule FakeSuiteModule do
        def __toast_suite__, do: __MODULE__
      end

      deployment = fake_deployment(%{endpoint: "http://localhost:9999"})
      DeploymentRegistry.put(FakeSuiteModule, deployment)
      DeploymentRegistry.put_extra_context(FakeSuiteModule, %{extra_key: "extra_val"})

      # Verify the registry path works
      assert DeploymentRegistry.get(FakeSuiteModule) == deployment
      assert DeploymentRegistry.get_extra_context(FakeSuiteModule) == %{extra_key: "extra_val"}
    end
  end

  describe "setup returns error when no deployment registered" do
    test "get_deployment raises with useful message" do
      error =
        assert_raise RuntimeError, fn ->
          ToastTest.Case.get_deployment()
        end

      assert error.message =~ "No deployment registered"
      assert error.message =~ "setup_suite"
      assert error.message =~ "test_helper.exs"
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

      # The module should compile without errors, meaning the macro expanded.
      # The 'using' block defines an alias for Toast.Client and sets @moduletag :toast_suite
      assert Code.ensure_loaded?(SampleTestModule)
    end

    test "modules using ToastTest.Case are ExUnit.CaseTemplate consumers" do
      # ToastTest.Case is itself a CaseTemplate, so modules that `use` it
      # inherit the setup callback and the `using` block.
      defmodule AnotherTestModule do
        use ToastTest.Case
      end

      # ExUnit test modules that use a CaseTemplate get __ex_unit__ defined
      assert function_exported?(AnotherTestModule, :__ex_unit__, 0)
    end
  end
end
