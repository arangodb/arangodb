defmodule ToastTest.Abort do
  @moduledoc "Manages suite-level abort state via an ETS table."

  @table :toast_suite_abort
  @prefix "Suite aborted: "

  @doc "Returns the abort-skipped message prefix."
  @spec prefix() :: String.t()
  def prefix, do: @prefix

  @doc """
  Aborts the current suite run with the given reason.

  Only the first call takes effect (subsequent calls are no-ops).
  Prints a red banner to stdout on the first abort.
  """
  @spec abort!(String.t() | {atom(), String.t()}) :: :ok
  def abort!(reason) do
    bar = String.duplicate("\u2550", 80)

    if :ets.insert_new(@table, {:aborted, reason}) do
      IO.puts(
        IO.ANSI.format([
          :red,
          bar,
          "\n ",
          display_reason(reason),
          "\n ABORTING FURTHER TESTS\n",
          bar,
          "\n",
          :reset
        ])
      )
    end

    :ok
  end

  @doc "Clears the abort state (re-creates the ETS table)."
  @spec clear!() :: :ok
  def clear! do
    :ets.delete(@table)
  catch
    :error, :badarg -> :ok
  after
    :ets.new(@table, [:named_table, :set, :public])
    :ok
  end

  @doc "Register the currently running test process so it can be killed on abort."
  @spec register_test_pid(pid()) :: :ok
  def register_test_pid(pid) do
    :ets.insert(@table, {:test_pid, pid})
    :ok
  catch
    :error, :badarg -> :ok
  end

  @doc "Clear the registered test process."
  @spec unregister_test_pid() :: :ok
  def unregister_test_pid do
    :ets.delete(@table, :test_pid)
    :ok
  catch
    :error, :badarg -> :ok
  end

  @doc "Kill the currently registered test process, if any."
  @spec kill_test_pid() :: :ok
  def kill_test_pid do
    case :ets.lookup(@table, :test_pid) do
      [{:test_pid, pid}] -> Process.exit(pid, :kill)
      [] -> :ok
    end

    :ok
  catch
    :error, :badarg -> :ok
  end

  @doc "Returns the abort reason, or nil if not aborted."
  @spec reason() :: String.t() | {atom(), String.t()} | nil
  def reason do
    case :ets.lookup(@table, :aborted) do
      [{:aborted, reason}] -> reason
      [] -> nil
    end
  catch
    :error, :badarg -> nil
  end

  @doc "Extracts a human-readable message from an abort reason."
  @spec display_reason(term()) :: String.t()
  def display_reason({_type, msg}), do: msg
  def display_reason(msg) when is_binary(msg), do: msg
  def display_reason(other), do: inspect(other)

  @doc "Formats an abort reason as a skip message with the standard prefix."
  @spec format_skip(term()) :: String.t()
  def format_skip(reason), do: prefix() <> display_reason(reason)
end
