defmodule ToastTest.SuiteTest do
  use ExUnit.Case, async: true

  test "use ToastTest.Suite defines deployment_config/0 with defaults" do
    defmodule DefaultSuite do
      use ToastTest.Suite
    end

    config = DefaultSuite.deployment_config()
    assert Keyword.get(config, :mode) == :auto
    assert Keyword.get(config, :timeout) == 3_600_000
  end

  test "use ToastTest.Suite accepts custom options" do
    defmodule CustomSuite do
      use ToastTest.Suite,
        mode: :cluster,
        timeout: 600_000,
        server_args: ["--javascript.enabled", "true"],
        coordinator_args: ["--extra", "coord"],
        dbserver_args: ["--extra", "db"],
        agent_args: ["--extra", "agent"],
        cluster_dbservers: 3,
        cluster_coordinators: 2
    end

    config = CustomSuite.deployment_config()
    assert Keyword.get(config, :mode) == :cluster
    assert Keyword.get(config, :timeout) == 600_000
    assert Keyword.get(config, :server_args) == ["--javascript.enabled", "true"]
    assert Keyword.get(config, :coordinator_args) == ["--extra", "coord"]
    assert Keyword.get(config, :dbserver_args) == ["--extra", "db"]
    assert Keyword.get(config, :agent_args) == ["--extra", "agent"]
    assert Keyword.get(config, :cluster_dbservers) == 3
    assert Keyword.get(config, :cluster_coordinators) == 2
  end

  test "suite module implements ToastTest.Suite behaviour" do
    defmodule BehaviourSuite do
      use ToastTest.Suite
    end

    behaviours = BehaviourSuite.__info__(:attributes)[:behaviour] || []
    assert ToastTest.Suite in behaviours
  end

  test "suite module is a CaseTemplate with __using__ macro" do
    defmodule TemplateSuite do
      use ToastTest.Suite
    end

    assert macro_exported?(TemplateSuite, :__using__, 1)
  end

  test "test module using suite gets __toast_suite__/0" do
    defmodule ParentSuite do
      use ToastTest.Suite
    end

    defmodule TestUsingParent do
      use ToastTest.SuiteTest.ParentSuite
    end

    assert TestUsingParent.__toast_suite__() == ToastTest.SuiteTest.ParentSuite
  end

  test "test module using suite gets @toast_suite attribute" do
    defmodule AttrSuite do
      use ToastTest.Suite
    end

    defmodule TestWithAttr do
      use ToastTest.SuiteTest.AttrSuite

      def get_toast_suite, do: @toast_suite
    end

    assert TestWithAttr.get_toast_suite() == ToastTest.SuiteTest.AttrSuite
  end

  test "optional callbacks setup_deployment/1 and teardown_deployment/1" do
    defmodule MinimalSuite do
      use ToastTest.Suite
    end

    refute function_exported?(MinimalSuite, :setup_deployment, 1)
    refute function_exported?(MinimalSuite, :teardown_deployment, 1)
  end

  test "suite can define setup_deployment/1" do
    defmodule SetupSuite do
      use ToastTest.Suite

      def setup_deployment(_deployment) do
        {:ok, %{custom_key: "value"}}
      end
    end

    assert function_exported?(SetupSuite, :setup_deployment, 1)
    assert {:ok, %{custom_key: "value"}} = SetupSuite.setup_deployment(:fake)
  end

  test "role-specific args take precedence in merge" do
    defmodule MergeSuite do
      use ToastTest.Suite,
        server_args: ["--common", "arg"],
        coordinator_args: ["--coord-only", "yes"]
    end

    config = MergeSuite.deployment_config()
    assert Keyword.get(config, :server_args) == ["--common", "arg"]
    assert Keyword.get(config, :coordinator_args) == ["--coord-only", "yes"]
  end
end
