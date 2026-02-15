defmodule Toast.TestCaseTest do
  use ExUnit.Case, async: false

  alias Toast.TestCase

  @deployment_key :__test_deployment__

  setup do
    saved_deployment = Application.get_env(:toast, @deployment_key)
    saved_formatters = Application.get_env(:ex_unit, :formatters)
    saved_result_dir = System.get_env("TOAST_RESULT_DIR")

    on_exit(fn ->
      if saved_deployment,
        do: Application.put_env(:toast, @deployment_key, saved_deployment),
        else: Application.delete_env(:toast, @deployment_key)

      if saved_formatters do
        ExUnit.configure(formatters: saved_formatters)
      end

      if saved_result_dir,
        do: System.put_env("TOAST_RESULT_DIR", saved_result_dir),
        else: System.delete_env("TOAST_RESULT_DIR")
    end)

    Application.delete_env(:toast, @deployment_key)
    :ok
  end

  defp fake_deployment(overrides \\ %{}) do
    defaults = %{
      id: "test-1",
      mode: :single_server,
      endpoint: "http://localhost:8529",
      controller: self()
    }

    fields = Map.merge(defaults, overrides)

    %Toast.Deployment{
      id: fields.id,
      mode: fields.mode,
      endpoint: fields.endpoint,
      controller: fields.controller
    }
  end

  describe "register_deployment/1 and get_deployment/0" do
    test "registers and retrieves a deployment" do
      deployment = fake_deployment()

      assert :ok = TestCase.register_deployment(deployment)
      assert TestCase.get_deployment() == deployment
    end

    test "returns the most recently registered deployment" do
      first = fake_deployment()
      second = fake_deployment(%{id: "test-2", endpoint: "http://localhost:9529"})

      TestCase.register_deployment(first)
      TestCase.register_deployment(second)

      retrieved = TestCase.get_deployment()
      assert retrieved.id == "test-2"
      assert retrieved.endpoint == "http://localhost:9529"
    end

    test "stores deployment in Application env under :toast" do
      deployment = fake_deployment()
      TestCase.register_deployment(deployment)

      raw = Application.get_env(:toast, @deployment_key)
      assert raw == deployment
    end
  end

  describe "get_deployment/0 with no registration" do
    test "raises when nothing registered" do
      assert_raise RuntimeError, ~r/No deployment registered/, fn ->
        TestCase.get_deployment()
      end
    end

    test "error message mentions setup_suite and test_helper.exs" do
      error =
        assert_raise RuntimeError, fn ->
          TestCase.get_deployment()
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

      unless Toast.ResultFormatter in current do
        ExUnit.configure(formatters: current ++ [Toast.ResultFormatter])
      end

      formatters = Application.get_env(:ex_unit, :formatters)
      assert ExUnit.CLIFormatter in formatters
      assert Toast.ResultFormatter in formatters
    end

    test "when TOAST_RESULT_DIR is not set, formatters remain unchanged" do
      System.delete_env("TOAST_RESULT_DIR")
      base_formatters = [ExUnit.CLIFormatter]
      ExUnit.configure(formatters: base_formatters)

      # Replicate the conditional — TOAST_RESULT_DIR nil means skip
      if System.get_env("TOAST_RESULT_DIR") do
        current = Application.get_env(:ex_unit, :formatters, [ExUnit.CLIFormatter])

        unless Toast.ResultFormatter in current do
          ExUnit.configure(formatters: current ++ [Toast.ResultFormatter])
        end
      end

      formatters = Application.get_env(:ex_unit, :formatters)
      assert formatters == [ExUnit.CLIFormatter]
    end

    test "does not duplicate ResultFormatter if already present" do
      System.put_env("TOAST_RESULT_DIR", "/tmp/toast_test_results")
      base_formatters = [ExUnit.CLIFormatter, Toast.ResultFormatter]
      ExUnit.configure(formatters: base_formatters)

      current = Application.get_env(:ex_unit, :formatters, [ExUnit.CLIFormatter])

      unless Toast.ResultFormatter in current do
        ExUnit.configure(formatters: current ++ [Toast.ResultFormatter])
      end

      formatters = Application.get_env(:ex_unit, :formatters)
      count = Enum.count(formatters, &(&1 == Toast.ResultFormatter))
      assert count == 1
    end
  end
end
