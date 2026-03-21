defmodule ToastTest.StateCleanup do
  @moduledoc "Resets all shared state (registries, ETS tables, formatters) between suite runs."

  @spec reset() :: :ok
  def reset do
    reset_deployment_registry()
    reset_abort_table()
    reset_after_suite_callbacks()
    reset_event_store()
  end

  defp reset_deployment_registry do
    ToastTest.DeploymentRegistry.clear()
  catch
    :error, :badarg -> :ok
  end

  defp reset_abort_table do
    ToastTest.Abort.clear!()
  end

  defp reset_after_suite_callbacks do
    Application.put_env(:ex_unit, :after_suite, [])
  end

  defp reset_event_store do
    ToastTest.EventStore.clear()
  catch
    :exit, _ -> :ok
  end
end
