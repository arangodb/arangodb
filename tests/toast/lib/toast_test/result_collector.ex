################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

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

  @spec notify_between_tests_finished(pid(), ExUnit.Test.t()) :: :ok
  def notify_between_tests_finished(pid, %ExUnit.Test{} = test) do
    GenServer.cast(pid, {:between_tests_finished, test})
  end

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
