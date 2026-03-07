defmodule ToastTest.ResultFormatter do
  @moduledoc "ExUnit formatter that collects test results for structured export."

  use GenServer

  defmodule Results do
    @moduledoc "Collected test results from a suite run."

    @type t :: %__MODULE__{
            started_at: DateTime.t(),
            finished_at: DateTime.t() | nil,
            times_us: map() | nil,
            modules: %{module() => %{tests: [ToastTest.TestResult.t()]}},
            failures: [ExUnit.Test.t()]
          }

    @enforce_keys [:started_at]
    defstruct [
      :started_at,
      :finished_at,
      :times_us,
      modules: %{},
      failures: []
    ]
  end

  # --- State ---

  defmodule State do
    @moduledoc false

    @type t :: %__MODULE__{
            suite_started_at: DateTime.t(),
            results: Results.t() | nil,
            modules: %{module() => [ToastTest.TestResult.t()]},
            test_start_times: %{{module(), atom()} => DateTime.t()},
            failures: [ExUnit.Test.t()],
            config: keyword()
          }

    defstruct [
      :suite_started_at,
      :results,
      modules: %{},
      test_start_times: %{},
      failures: [],
      config: []
    ]
  end

  # --- Client API ---

  @spec get_results(pid()) :: Results.t() | nil
  def get_results(pid), do: GenServer.call(pid, :get_results)

  @doc false
  def init(opts) do
    {:ok, %State{suite_started_at: DateTime.utc_now(), config: opts}}
  end

  # --- Event handling ---

  @doc false
  def handle_cast({:suite_started, _opts}, state) do
    {:noreply, %{state | suite_started_at: DateTime.utc_now()}}
  end

  def handle_cast({:test_started, %ExUnit.Test{} = test}, state) do
    key = {test.module, test.name}
    {:noreply, put_in(state.test_start_times[key], DateTime.utc_now())}
  end

  def handle_cast({:test_finished, %ExUnit.Test{} = test}, state) do
    key = {test.module, test.name}
    started_at = state.test_start_times[key]
    result = extract_test_result(test, started_at)
    modules = Map.update(state.modules, test.module, [result], &[result | &1])
    failures = record_failure(state.failures, test)
    {:noreply, %{state | modules: modules, failures: failures}}
  end

  def handle_cast({:suite_finished, times_us}, state) do
    modules =
      Map.new(state.modules, fn {mod, tests} ->
        tests = Enum.reverse(tests)

        {mod,
         %{
           tests: tests,
           started_at:
             tests |> Enum.map(& &1.started_at) |> Enum.reject(&is_nil/1) |> min_datetime(),
           finished_at:
             tests |> Enum.map(& &1.finished_at) |> Enum.reject(&is_nil/1) |> max_datetime()
         }}
      end)

    results = %Results{
      started_at: state.suite_started_at,
      finished_at: DateTime.utc_now(),
      times_us: times_us,
      modules: modules,
      failures: Enum.reverse(state.failures)
    }

    {:noreply, %{state | results: results}}
  end

  def handle_cast(_msg, state) do
    {:noreply, state}
  end

  def handle_call(:get_results, _from, state) do
    {:reply, state.results, state}
  end

  @doc "Flatten hierarchical module results to a flat test list."
  def flat_tests(%{modules: modules}) do
    Enum.flat_map(modules, fn {_mod, %{tests: tests}} -> tests end)
  end

  defp min_datetime([]), do: nil
  defp min_datetime(dts), do: Enum.min(dts, DateTime)

  defp max_datetime([]), do: nil
  defp max_datetime(dts), do: Enum.max(dts, DateTime)

  # --- Result extraction ---

  defp extract_test_result(%ExUnit.Test{} = test, started_at) do
    {outcome, failure} = extract_outcome(test.state)
    finished_at = if started_at, do: DateTime.add(started_at, test.time, :microsecond)

    %ToastTest.TestResult{
      module: test.module,
      name: to_string(test.name),
      outcome: outcome,
      duration_us: test.time,
      failure: failure,
      started_at: started_at,
      finished_at: finished_at,
      tags: %{
        file: test.tags[:file],
        line: test.tags[:line]
      }
    }
  end

  defp extract_outcome(nil), do: {:passed, nil}

  defp extract_outcome({:failed, failures}) do
    failure_info = Enum.map(failures, &format_failure/1)
    {:failed, failure_info}
  end

  defp extract_outcome({:skipped, msg}), do: {:skipped, %{message: msg}}
  defp extract_outcome({:excluded, msg}), do: {:excluded, %{message: msg}}
  defp extract_outcome({:invalid, _module}), do: {:invalid, nil}

  defp format_failure({:error, %{__struct__: mod} = error, stack}) do
    %{
      kind: inspect(mod),
      message: Exception.message(error),
      stacktrace: Exception.format_stacktrace(stack)
    }
  end

  defp format_failure({:error, reason, stack}) do
    %{
      kind: "ErlangError",
      message: inspect(reason),
      stacktrace: Exception.format_stacktrace(stack)
    }
  end

  defp format_failure({:exit, reason, stack}) do
    %{
      kind: "exit",
      message: inspect(reason),
      stacktrace: Exception.format_stacktrace(stack)
    }
  end

  defp format_failure({:throw, value, stack}) do
    %{
      kind: "throw",
      message: inspect(value),
      stacktrace: Exception.format_stacktrace(stack)
    }
  end

  # Linked process exit (includes server crash propagated via crash_monitor)
  defp format_failure({{:EXIT, pid}, reason, stack}) do
    %{
      kind: "EXIT",
      message: "linked process #{inspect(pid)} exited: #{inspect(reason)}",
      stacktrace: Exception.format_stacktrace(stack)
    }
  end

  defp record_failure(failures, %ExUnit.Test{state: {:failed, _}} = test), do: [test | failures]
  defp record_failure(failures, _test), do: failures
end
