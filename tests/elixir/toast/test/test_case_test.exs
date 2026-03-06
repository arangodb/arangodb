defmodule ToastTest.CaseTest do
  use ExUnit.Case, async: false

  alias ToastTest.DeploymentRegistry

  describe "DeploymentRegistry integration" do
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

    test "put and get roundtrip" do
      deployment = fake_deployment()
      assert :ok = DeploymentRegistry.put(MySuite, deployment)
      assert DeploymentRegistry.get(MySuite) == deployment
    end

    test "extra context roundtrip" do
      extra = %{agency_size: 3, replication_factor: 2}
      assert :ok = DeploymentRegistry.put_extra_context(MySuite, extra)
      assert DeploymentRegistry.get_extra_context(MySuite) == extra
    end

    test "get raises for missing suite" do
      assert_raise RuntimeError, ~r/No deployment registered/, fn ->
        DeploymentRegistry.get(NonexistentSuite)
      end
    end

    test "get_extra_context returns empty map for missing suite" do
      assert DeploymentRegistry.get_extra_context(NonexistentSuite) == %{}
    end

    test "clear removes all entries" do
      deployment = fake_deployment()
      DeploymentRegistry.put(MySuite, deployment)
      DeploymentRegistry.put_extra_context(MySuite, %{foo: :bar})

      assert :ok = DeploymentRegistry.clear()

      assert_raise RuntimeError, ~r/No deployment registered/, fn ->
        DeploymentRegistry.get(MySuite)
      end

      assert DeploymentRegistry.get_extra_context(MySuite) == %{}
    end
  end

  defp fake_deployment(overrides \\ %{}) do
    defaults = %{
      id: "test-1",
      mode: :single_server,
      config: Toast.Config.load(),
      endpoint: "http://localhost:8529",
      controller: self()
    }

    fields = Map.merge(defaults, overrides)

    %Toast.Deployment{
      id: fields.id,
      mode: fields.mode,
      config: fields.config,
      endpoint: fields.endpoint,
      controller: fields.controller
    }
  end
end
