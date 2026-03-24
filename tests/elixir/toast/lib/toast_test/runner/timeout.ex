defmodule ToastTest.Runner.Timeout do
  @moduledoc false

  alias ToastTest.{Abort, EventStore}

  require Logger

  defmodule Settings do
    @moduledoc false
    @enforce_keys [:base_timeout, :timeout_factor, :suite_deadline, :trace]
    defstruct [:base_timeout, :timeout_factor, :suite_deadline, :trace]

    @type t :: %__MODULE__{
            base_timeout: pos_integer() | nil,
            timeout_factor: number(),
            suite_deadline: integer() | nil,
            trace: boolean()
          }
  end

  def get_timeout(config, tags) do
    ts = config.timeout_settings

    ts
    |> compute_base_timeout(tags)
    |> apply_timeout_factor(ts.timeout_factor)
    |> clamp_to_deadline(ts.suite_deadline)
  end

  def compute_suite_deadline(suite_timeout, nil) do
    System.monotonic_time(:millisecond) + suite_timeout
  end

  def compute_suite_deadline(suite_timeout, global_deadline) do
    now = System.monotonic_time(:millisecond)
    suite_end = now + suite_timeout
    min(suite_end, global_deadline)
  end

  def check_suite_deadline!(%{timeout_settings: %{suite_deadline: nil}}), do: :ok

  def check_suite_deadline!(%{timeout_settings: %{suite_deadline: deadline}}) do
    if System.monotonic_time(:millisecond) >= deadline do
      abort_with_timeout(:test_timeout, "Suite timeout exceeded")
    end
  end

  def check_global_deadline!(nil), do: :ok

  def check_global_deadline!(deadline) do
    if System.monotonic_time(:millisecond) >= deadline do
      abort_with_timeout(:global_timeout, "Global execution timeout exceeded")
    end
  end

  defp compute_base_timeout(%{trace: true}, _tags), do: :infinity
  defp compute_base_timeout(ts, tags), do: Map.get(tags, :timeout, ts.base_timeout)

  defp apply_timeout_factor(:infinity, _factor), do: :infinity
  defp apply_timeout_factor(timeout, factor), do: round(timeout * factor)

  defp clamp_to_deadline(timeout, nil), do: timeout

  defp clamp_to_deadline(timeout, deadline) do
    remaining = max(deadline - System.monotonic_time(:millisecond), 1)
    if timeout == :infinity, do: remaining, else: min(timeout, remaining)
  end

  defp abort_with_timeout(source, reason) do
    Logger.warning("#{reason} — aborting suite")
    EventStore.record_timeout_kill(source, reason, [])
    Abort.abort!({:timeout, reason})
  end
end
