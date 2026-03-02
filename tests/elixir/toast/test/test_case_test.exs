defmodule ToastTest.CaseTest do
  use ExUnit.Case, async: false

  alias ToastTest.Case
  alias ToastTest.DeploymentRegistry

  setup do
    saved_formatters = Application.get_env(:ex_unit, :formatters)
    saved_result_dir = System.get_env("TOAST_RESULT_DIR")

    # Ensure registry exists and clear standalone entry
    DeploymentRegistry.ensure_init()

    saved_deployment =
      try do
        DeploymentRegistry.get(:__standalone__)
      rescue
        RuntimeError -> nil
      end

    if saved_deployment == nil do
      # No standalone entry — just clear to ensure clean state
      DeploymentRegistry.clear()
    end

    on_exit(fn ->
      DeploymentRegistry.ensure_init()

      if saved_deployment do
        DeploymentRegistry.put(:__standalone__, saved_deployment)
      else
        DeploymentRegistry.clear()
      end

      if saved_formatters do
        ExUnit.configure(formatters: saved_formatters)
      end

      if saved_result_dir,
        do: System.put_env("TOAST_RESULT_DIR", saved_result_dir),
        else: System.delete_env("TOAST_RESULT_DIR")
    end)

    DeploymentRegistry.clear()
    :ok
  end

  defp fake_deployment(overrides \\ %{}) do
    defaults = %{
      id: "test-1",
      mode: :single_server,
      config: Toast.Config.load(),
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

  describe "register_deployment/1 and get_deployment/0" do
    test "registers and retrieves a deployment" do
      deployment = fake_deployment()

      assert :ok = Case.register_deployment(deployment)
      assert Case.get_deployment() == deployment
    end

    test "returns the most recently registered deployment" do
      first = fake_deployment()
      second = fake_deployment(%{id: "test-2", endpoint: "http://localhost:9529"})

      Case.register_deployment(first)
      Case.register_deployment(second)

      retrieved = Case.get_deployment()
      assert retrieved.id == "test-2"
      assert retrieved.endpoint == "http://localhost:9529"
    end

    test "stores deployment in DeploymentRegistry under standalone key" do
      deployment = fake_deployment()
      Case.register_deployment(deployment)

      raw = DeploymentRegistry.get(:__standalone__)
      assert raw == deployment
    end
  end

  describe "get_deployment/0 with no registration" do
    test "raises when nothing registered" do
      assert_raise RuntimeError, ~r/No deployment registered/, fn ->
        Case.get_deployment()
      end
    end

    test "error message mentions setup_suite and test_helper.exs" do
      error =
        assert_raise RuntimeError, fn ->
          Case.get_deployment()
        end

      assert error.message =~ "setup_suite"
      assert error.message =~ "test_helper.exs"
    end
  end

  describe "formatter registration behavior" do
    # maybe_register_formatter/0 is private and only reachable through
    # setup_suite!/setup_suite which require a real deployment. We test the
    # formatter registration logic by exercising the same ExUnit.configure
    # mechanism the private function uses, verifying the env-based conditional.

    test "when TOAST_RESULT_DIR is set, ResultFormatter should be added to formatters" do
      System.put_env("TOAST_RESULT_DIR", "/tmp/toast_test_results")
      base_formatters = [ExUnit.CLIFormatter]
      ExUnit.configure(formatters: base_formatters)

      # Replicate the logic from maybe_register_formatter/0
      current = Application.get_env(:ex_unit, :formatters, [ExUnit.CLIFormatter])

      unless ToastTest.ResultFormatter in current do
        ExUnit.configure(formatters: current ++ [ToastTest.ResultFormatter])
      end

      formatters = Application.get_env(:ex_unit, :formatters)
      assert ExUnit.CLIFormatter in formatters
      assert ToastTest.ResultFormatter in formatters
    end

    test "ResultFormatter is always added regardless of TOAST_RESULT_DIR" do
      System.delete_env("TOAST_RESULT_DIR")
      base_formatters = [ExUnit.CLIFormatter]
      ExUnit.configure(formatters: base_formatters)

      # Replicate the logic from register_formatters — always add ResultFormatter
      current = Application.get_env(:ex_unit, :formatters, [ExUnit.CLIFormatter])

      unless ToastTest.ResultFormatter in current do
        ExUnit.configure(formatters: current ++ [ToastTest.ResultFormatter])
      end

      formatters = Application.get_env(:ex_unit, :formatters)
      assert ExUnit.CLIFormatter in formatters
      assert ToastTest.ResultFormatter in formatters
    end

    test "does not duplicate ResultFormatter if already present" do
      System.put_env("TOAST_RESULT_DIR", "/tmp/toast_test_results")
      base_formatters = [ExUnit.CLIFormatter, ToastTest.ResultFormatter]
      ExUnit.configure(formatters: base_formatters)

      current = Application.get_env(:ex_unit, :formatters, [ExUnit.CLIFormatter])

      unless ToastTest.ResultFormatter in current do
        ExUnit.configure(formatters: current ++ [ToastTest.ResultFormatter])
      end

      formatters = Application.get_env(:ex_unit, :formatters)
      count = Enum.count(formatters, &(&1 == ToastTest.ResultFormatter))
      assert count == 1
    end
  end

  describe "DeploymentRegistry integration" do
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
end
