defmodule ToastTest.StateCleanup do
  @moduledoc "Resets all shared state between suite runs."

  @spec reset() :: :ok
  def reset do
    ToastTest.DeploymentRegistry.clear()
    ToastTest.Abort.clear!()
    ToastTest.ExUnitCompat.clear_after_suite()
    ToastTest.EventStore.clear()
  end
end
