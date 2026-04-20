defmodule ToastTest.Runner.Timeout do
  @moduledoc false

  alias ToastTest.{Abort, EventStore}

  require Logger

  defmodule Settings do
    @moduledoc false
    @enforce_keys [:base_timeout, :timeout_factor, :suite_deadline, :disable_timeouts]

    defstruct [
      :base_timeout,
      :timeout_factor,
      :suite_deadline,
      :suite_timeout,
      :global_deadline,
      :global_timeout,
      :disable_timeouts
    ]

    @type t :: %__MODULE__{
            base_timeout: pos_integer() | nil,
            timeout_factor: number(),
            suite_deadline: integer() | nil,
            suite_timeout: pos_integer() | nil,
            global_deadline: integer() | nil,
            global_timeout: pos_integer() | nil,
            disable_timeouts: boolean()
          }
  end

  @type timeout_source ::
          :test
          | {:global_deadline, configured_timeout :: pos_integer()}
          | {:suite_deadline, configured_timeout :: pos_integer()}

  @spec get_timeout(map(), map()) :: {pos_integer() | :infinity, timeout_source()}
  def get_timeout(config, tags) do
    ts = config.timeout_settings

    timeout =
      ts
      |> compute_base_timeout(tags)
      |> apply_timeout_factor(ts.timeout_factor)

    timeout
    |> clamp_to_deadline(ts.global_deadline, {:global_deadline, ts.global_timeout})
    |> clamp_to_deadline(ts.suite_deadline, {:suite_deadline, ts.suite_timeout})
    |> resolve_source()
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

  defp compute_base_timeout(%{disable_timeouts: true}, _tags), do: :infinity
  defp compute_base_timeout(ts, tags), do: Map.get(tags, :timeout, ts.base_timeout)

  defp apply_timeout_factor(:infinity, _factor), do: :infinity
  defp apply_timeout_factor(timeout, factor), do: round(timeout * factor)

  defp clamp_to_deadline(timeout, nil, _source), do: timeout

  defp clamp_to_deadline(timeout, deadline, source) do
    remaining = max(deadline - System.monotonic_time(:millisecond), 1)
    current = effective_ms(timeout)

    if remaining < current,
      do: {remaining, source},
      else: timeout
  end

  defp effective_ms({ms, _source}), do: ms
  defp effective_ms(:infinity), do: :infinity
  defp effective_ms(ms), do: ms

  defp resolve_source({clamped_ms, source}), do: {clamped_ms, source}
  defp resolve_source(timeout), do: {timeout, :test}

  defp abort_with_timeout(source, reason) do
    Logger.warning("#{reason} — aborting suite")
    EventStore.record_timeout_kill(source, reason, [])
    Abort.abort!({:timeout, reason})
  end
end
