defmodule ToastTest.ResultCollector do
  @moduledoc "ExUnit formatter that collects test results with module-level timestamp tracking."

  use GenServer

  @type test_data :: %{
          suite: String.t() | nil,
          started_at: DateTime.t(),
          finished_at: DateTime.t() | nil,
          times_us: map() | nil,
          modules: %{module() => ToastTest.SuiteResult.module_result()},
          failures: [ExUnit.Test.t()]
        }

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

  # --- Client API ---

  @spec get_data(pid()) :: test_data()
  def get_data(pid), do: GenServer.call(pid, :get_data)

  # --- GenServer callbacks ---

  @doc false
  def init(opts) do
    {:ok, %__MODULE__{suite_started_at: DateTime.utc_now(), config: opts}}
  end

  @doc false
  def handle_cast({:suite_started, _opts}, state) do
    {:noreply, %{state | suite_started_at: DateTime.utc_now()}}
  end

  def handle_cast({:module_started, %ExUnit.TestModule{name: module}}, state) do
    timestamps =
      Map.put_new(state.module_timestamps, module, %{
        started_at: DateTime.utc_now(),
        finished_at: nil,
        setup_finished_at: nil,
        teardown_started_at: nil
      })

    {:noreply, %{state | module_timestamps: timestamps}}
  end

  def handle_cast({:module_finished, %ExUnit.TestModule{name: module}}, state) do
    timestamps =
      Map.update!(state.module_timestamps, module, fn ts ->
        %{ts | finished_at: DateTime.utc_now()}
      end)

    {:noreply, %{state | module_timestamps: timestamps}}
  end

  def handle_cast({:test_started, %ExUnit.Test{} = test}, state) do
    now = DateTime.utc_now()
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

    {:noreply,
     %{state | test_start_times: test_start_times, module_timestamps: module_timestamps}}
  end

  def handle_cast({:test_finished, %ExUnit.Test{} = test}, state) do
    now = DateTime.utc_now()
    key = {test.module, test.name}
    started_at = state.test_start_times[key]
    finished_at = if started_at, do: DateTime.add(started_at, test.time, :microsecond)

    result = %{
      name: test.name,
      outcome: extract_outcome(test.state),
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

    {:noreply,
     %{state | modules: modules, failures: failures, module_timestamps: module_timestamps}}
  end

  def handle_cast({:suite_finished, times_us}, state) do
    {:noreply, %{state | finished_at: DateTime.utc_now(), times_us: times_us}}
  end

  def handle_cast(_msg, state) do
    {:noreply, state}
  end

  @doc false
  def handle_call(:get_data, _from, state) do
    data = %{
      suite: state.config[:suite],
      started_at: state.suite_started_at,
      finished_at: state.finished_at,
      times_us: state.times_us,
      modules: build_modules(state.modules, state.module_timestamps),
      failures: Enum.reverse(state.failures)
    }

    {:reply, data, state}
  end

  # --- Private helpers ---

  defp build_modules(modules_map, module_timestamps) do
    modules_map
    |> Map.keys()
    |> Enum.concat(Map.keys(module_timestamps))
    |> Enum.uniq()
    |> Map.new(fn mod ->
      tests = modules_map |> Map.get(mod, []) |> Enum.reverse()
      ts = Map.get(module_timestamps, mod)

      {started_at, finished_at} =
        if ts do
          {ts.started_at, ts.finished_at}
        else
          # Fallback to test timestamps
          started = tests |> Enum.map(& &1.started_at) |> Enum.reject(&is_nil/1) |> min_dt()
          finished = tests |> Enum.map(& &1.finished_at) |> Enum.reject(&is_nil/1) |> max_dt()
          {started, finished}
        end

      {setup_finished_at, teardown_started_at} =
        if ts, do: {ts.setup_finished_at, ts.teardown_started_at}, else: {nil, nil}

      {mod,
       %{
         started_at: started_at,
         finished_at: finished_at,
         setup_finished_at: setup_finished_at,
         teardown_started_at: teardown_started_at,
         tests: tests
       }}
    end)
  end

  defp min_dt([]), do: nil
  defp min_dt(dts), do: Enum.min(dts, DateTime)

  defp max_dt([]), do: nil
  defp max_dt(dts), do: Enum.max(dts, DateTime)

  defp extract_outcome(nil), do: :passed
  defp extract_outcome({:failed, _}), do: :failed
  defp extract_outcome({:skipped, _}), do: :skipped
  defp extract_outcome({:excluded, _}), do: :excluded
  defp extract_outcome({:invalid, _}), do: :invalid

  defp record_failure(failures, %ExUnit.Test{state: {:failed, _}} = test), do: [test | failures]
  defp record_failure(failures, _test), do: failures
end
