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
    if :ets.insert_new(@table, {:aborted, reason}) do
      IO.puts([
        IO.ANSI.red(),
        "====================================",
        "\n   ",
        display_reason(reason),
        "\n   !!! Aborting further tests !!!\n",
        "====================================\n",
        IO.ANSI.reset()
      ])
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
end
