defmodule ToastTest.DeploymentRegistry do
  @table :toast_deployment_registry

  @spec init() :: :ok
  def init do
    if :ets.whereis(@table) != :undefined do
      :ets.delete(@table)
    end

    :ets.new(@table, [:named_table, :public, :set])
    :ok
  end

  @spec put(module(), Toast.Deployment.t()) :: :ok
  def put(suite_module, deployment) do
    :ets.insert(@table, {suite_module, deployment})
    :ok
  end

  @spec get(module()) :: Toast.Deployment.t()
  def get(suite_module) do
    case :ets.lookup(@table, suite_module) do
      [{^suite_module, deployment}] -> deployment
      [] -> raise "No deployment registered for suite #{inspect(suite_module)}"
    end
  end

  @spec put_extra_context(module(), map()) :: :ok
  def put_extra_context(suite_module, extra_context) do
    :ets.insert(@table, {{suite_module, :extra_context}, extra_context})
    :ok
  end

  @spec get_extra_context(module()) :: map()
  def get_extra_context(suite_module) do
    case :ets.lookup(@table, {suite_module, :extra_context}) do
      [{{^suite_module, :extra_context}, ctx}] -> ctx
      [] -> %{}
    end
  end

  @spec clear() :: :ok
  def clear do
    :ets.delete_all_objects(@table)
    :ok
  end
end
