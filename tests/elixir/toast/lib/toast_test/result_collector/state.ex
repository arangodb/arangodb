defmodule ToastTest.ResultCollector.State do
  @moduledoc """
  Pure state logic for ResultCollector.

  All functions are side-effect free. Timestamps are passed in explicitly
  rather than calling DateTime.utc_now(), making the logic deterministic
  and easy to test.
  """

  import Toast.Utils, only: [compact: 1]

  defstruct [
    :suite_started_at,
    :finished_at,
    :times_us,
    modules: %{},
    module_timestamps: %{},
    test_start_times: %{},
    failures: [],
    config: []
  ]

  @doc "Create initial state with the given start timestamp and options."
  def new(now, opts \\ []) do
    %__MODULE__{suite_started_at: now, config: opts}
  end

  @doc "Apply an event to the state, returning updated state."
  def apply_event(state, {:suite_started, _opts, now}) do
    %{state | suite_started_at: now}
  end

  def apply_event(state, {:module_started, %ExUnit.TestModule{name: module}, now}) do
    timestamps =
      Map.put_new(state.module_timestamps, module, %{
        started_at: now,
        finished_at: nil,
        setup_finished_at: nil,
        teardown_started_at: nil
      })

    %{state | module_timestamps: timestamps}
  end

  def apply_event(state, {:module_finished, %ExUnit.TestModule{name: module}, now}) do
    timestamps =
      Map.update!(state.module_timestamps, module, &%{&1 | finished_at: now})

    %{state | module_timestamps: timestamps}
  end

  def apply_event(state, {:test_started, %ExUnit.Test{} = test, now}) do
    key = {test.module, test.name}
    test_start_times = Map.put(state.test_start_times, key, now)

    # First test for this module sets setup_finished_at
    mod = test.module

    module_timestamps =
      case state.module_timestamps do
        %{^mod => %{setup_finished_at: nil} = ts} ->
          Map.put(state.module_timestamps, mod, %{ts | setup_finished_at: now})

        _ ->
          state.module_timestamps
      end

    %{state | test_start_times: test_start_times, module_timestamps: module_timestamps}
  end

  def apply_event(state, {:test_finished, %ExUnit.Test{} = test, now}) do
    key = {test.module, test.name}
    started_at = state.test_start_times[key]
    finished_at = if started_at, do: DateTime.add(started_at, test.time, :microsecond)

    result = %{
      name: test.name,
      outcome: ToastTest.Formatting.test_outcome(test),
      duration_us: test.time,
      started_at: started_at,
      finished_at: finished_at,
      tags: %{file: test.tags[:file], line: test.tags[:line]}
    }

    modules = Map.update(state.modules, test.module, [result], &[result | &1])
    failures = record_failure(state.failures, test)

    # Update teardown_started_at to track last test_finished
    module_timestamps =
      case state.module_timestamps[test.module] do
        nil -> state.module_timestamps
        ts -> Map.put(state.module_timestamps, test.module, %{ts | teardown_started_at: now})
      end

    %{state | modules: modules, failures: failures, module_timestamps: module_timestamps}
  end

  def apply_event(state, {:suite_finished, times_us, now}) do
    %{state | finished_at: now, times_us: times_us}
  end

  def apply_event(state, _unknown), do: state

  @doc "Convert collector state into the public test_data format."
  def to_test_data(state) do
    %{
      suite: state.config[:suite],
      started_at: state.suite_started_at,
      finished_at: state.finished_at,
      times_us: state.times_us,
      modules: build_modules(state.modules, state.module_timestamps),
      failures: Enum.reverse(state.failures)
    }
  end

  # --- Private helpers ---

  defp build_modules(modules_map, module_timestamps) do
    (Map.keys(modules_map) ++ Map.keys(module_timestamps))
    |> Map.new(fn mod ->
      tests = modules_map |> Map.get(mod, []) |> Enum.reverse()
      {mod, build_module_result(tests, Map.get(module_timestamps, mod))}
    end)
  end

  defp build_module_result(tests, nil) do
    %{
      started_at: tests |> Enum.map(& &1.started_at) |> compact() |> min_dt(),
      finished_at: tests |> Enum.map(& &1.finished_at) |> compact() |> max_dt(),
      setup_finished_at: nil,
      teardown_started_at: nil,
      tests: tests
    }
  end

  defp build_module_result(tests, ts), do: Map.put(ts, :tests, tests)

  defp min_dt([]), do: nil
  defp min_dt(dts), do: Enum.min(dts, DateTime)

  defp max_dt([]), do: nil
  defp max_dt(dts), do: Enum.max(dts, DateTime)

  defp record_failure(failures, %ExUnit.Test{state: {:failed, _}} = test), do: [test | failures]
  defp record_failure(failures, _test), do: failures
end
