defmodule ToastTest.StateCleanup do
  @moduledoc "Resets all shared state (registries, ETS tables, formatters) between suite runs."

  @spec reset() :: :ok
  def reset do
    reset_deployment_registry()
    reset_abort_table()
    reset_after_suite_callbacks()
    reset_formatters()
    reset_process_history()
  end

  defp reset_deployment_registry do
    ToastTest.DeploymentRegistry.clear()
  rescue
    ArgumentError -> :ok
  end

  defp reset_abort_table do
    try do
      :ets.delete(:toast_suite_abort)
    catch
      :error, :badarg -> :ok
    end

    :ets.new(:toast_suite_abort, [:named_table, :set, :public])
    :ok
  end

  defp reset_after_suite_callbacks do
    Application.put_env(:ex_unit, :after_suite, [])
  end

  defp reset_formatters do
    formatters = Application.get_env(:ex_unit, :formatters, [])

    for formatter <- formatters,
        formatter not in [ExUnit.CLIFormatter, ToastTest.CLIFormatter],
        pid = GenServer.whereis(formatter),
        is_pid(pid) do
      GenServer.stop(pid, :normal, 1_000)
    end
  catch
    :exit, _ -> :ok
  end

  defp reset_process_history do
    if Process.whereis(ToastTest.ProcessHistory) do
      ToastTest.ProcessHistory.clear()
    end
  end
end
