defmodule ToastTest.DeploymentRegistry do
  @table :toast_deployment_registry

  def init do
    if :ets.whereis(@table) != :undefined do
      :ets.delete(@table)
    end

    :ets.new(@table, [:named_table, :public, :set])
    :ok
  end

  def put(suite_module, deployment) do
    :ets.insert(@table, {suite_module, deployment})
    :ok
  end

  def get(suite_module) do
    case :ets.lookup(@table, suite_module) do
      [{^suite_module, deployment}] -> deployment
      [] -> raise "No deployment registered for suite #{inspect(suite_module)}"
    end
  end

  def clear do
    :ets.delete_all_objects(@table)
    :ok
  end
end
