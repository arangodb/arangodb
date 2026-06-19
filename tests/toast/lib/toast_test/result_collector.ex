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
  @moduledoc "ExUnit formatter that collects test outcomes, durations, and failures."

  use GenServer

  alias ToastTest.ResultCollector.State

  @type collected_test :: %{
          name: atom(),
          outcome: atom(),
          duration_us: non_neg_integer(),
          tags: map()
        }

  @type test_data :: %{
          suite: String.t() | nil,
          times_us: map() | nil,
          modules: %{module() => %{tests: [collected_test()]}},
          failures: [ExUnit.Test.t()]
        }

  # --- Client API ---

  @spec get_data(pid()) :: test_data()
  def get_data(pid), do: GenServer.call(pid, :get_data)

  # --- GenServer callbacks ---

  @doc false
  def init(opts) do
    {:ok, State.new(opts)}
  end

  @doc false
  def handle_cast({event_type, payload}, state) do
    {:noreply, State.apply_event(state, {event_type, payload})}
  end

  @doc false
  def handle_call(:get_data, _from, state) do
    {:reply, State.to_test_data(state), state}
  end
end
