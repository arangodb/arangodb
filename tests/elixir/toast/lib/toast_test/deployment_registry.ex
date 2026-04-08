defmodule ToastTest.DeploymentRegistry do
  @moduledoc "Registry mapping suite modules to their active deployments."

  use Agent

  def start_link(_opts \\ []) do
    Agent.start_link(fn -> %{deployments: %{}, extra_contexts: %{}} end, name: __MODULE__)
  end

  @spec clear() :: :ok
  def clear do
    Agent.update(__MODULE__, fn _ -> %{deployments: %{}, extra_contexts: %{}} end)
  end

  @spec put(module(), Toast.Deployment.t()) :: :ok
  def put(suite_module, deployment) do
    Agent.update(__MODULE__, &put_in(&1, [:deployments, suite_module], deployment))
  end

  @spec get(module()) :: Toast.Deployment.t()
  def get(suite_module) do
    case Agent.get(__MODULE__, &get_in(&1, [:deployments, suite_module])) do
      nil -> raise "No deployment registered for suite #{inspect(suite_module)}"
      deployment -> deployment
    end
  end

  @spec put_extra_context(module(), map()) :: :ok
  def put_extra_context(suite_module, extra_context) do
    Agent.update(__MODULE__, &put_in(&1, [:extra_contexts, suite_module], extra_context))
  end

  @spec get_extra_context(module()) :: map()
  def get_extra_context(suite_module) do
    Agent.get(__MODULE__, &Map.get(&1.extra_contexts, suite_module, %{}))
  end
end
