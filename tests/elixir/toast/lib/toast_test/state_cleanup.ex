defmodule ToastTest.StateCleanup do
  @moduledoc "Resets all shared state between suite runs."

  @spec reset() :: :ok
  def reset do
    ToastTest.DeploymentRegistry.clear()
    ToastTest.Abort.clear!()
    Application.put_env(:ex_unit, :after_suite, [])
    ToastTest.EventStore.clear()
  end
end
