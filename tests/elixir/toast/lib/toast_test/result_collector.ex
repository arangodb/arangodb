defmodule ToastTest.ResultCollector do
  @moduledoc "ExUnit formatter that collects test results with module-level timestamp tracking."

  use GenServer

  alias ToastTest.ResultCollector.State

  @type test_data :: %{
          suite: String.t() | nil,
          started_at: DateTime.t(),
          finished_at: DateTime.t() | nil,
          times_us: map() | nil,
          modules: %{module() => ToastTest.SuiteResult.module_result()},
          failures: [ExUnit.Test.t()]
        }

  # --- Client API ---

  @spec get_data(pid()) :: test_data()
  def get_data(pid), do: GenServer.call(pid, :get_data)

  # --- GenServer callbacks ---

  @doc false
  def init(opts) do
    {:ok, State.new(DateTime.utc_now(), opts)}
  end

  @doc false
  def handle_cast({event_type, payload}, state) do
    {:noreply, State.apply_event(state, {event_type, payload, DateTime.utc_now()})}
  end

  @doc false
  def handle_call(:get_data, _from, state) do
    {:reply, State.to_test_data(state), state}
  end
end
