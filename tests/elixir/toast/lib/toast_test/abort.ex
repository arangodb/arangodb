defmodule ToastTest.Abort do
  @moduledoc "Manages suite-level abort state."

  use Agent

  @prefix "Suite aborted: "

  def start_link(_opts \\ []) do
    Agent.start_link(fn -> %{aborted: nil, test_pid: nil} end, name: __MODULE__)
  end

  @spec prefix() :: String.t()
  def prefix, do: @prefix

  @doc """
  Aborts the current suite run with the given reason.

  Only the first call takes effect (subsequent calls are no-ops).
  Prints a red banner to stdout on the first abort.
  """
  @spec abort!(String.t() | {atom(), String.t()}) :: :ok
  def abort!(reason) do
    first? =
      Agent.get_and_update(__MODULE__, fn
        %{aborted: nil} = s -> {true, %{s | aborted: reason}}
        s -> {false, s}
      end)

    if first? do
      bar = String.duplicate("\u2550", 80)

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

  @spec clear!() :: :ok
  def clear! do
    Agent.update(__MODULE__, fn _ -> %{aborted: nil, test_pid: nil} end)
  end

  @doc "Register the currently running test process so it can be killed on abort."
  @spec register_test_pid(pid()) :: :ok
  def register_test_pid(pid) do
    Agent.update(__MODULE__, &%{&1 | test_pid: pid})
  end

  @doc "Clear the registered test process."
  @spec unregister_test_pid() :: :ok
  def unregister_test_pid do
    Agent.update(__MODULE__, &%{&1 | test_pid: nil})
  end

  @doc "Kill the currently registered test process, if any."
  @spec kill_test_pid() :: :ok
  def kill_test_pid do
    if pid = Agent.get(__MODULE__, & &1.test_pid) do
      Process.exit(pid, :kill)
    end

    :ok
  end

  @doc "Returns the abort reason, or nil if not aborted."
  @spec reason() :: String.t() | {atom(), String.t()} | nil
  def reason do
    Agent.get(__MODULE__, & &1.aborted)
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
