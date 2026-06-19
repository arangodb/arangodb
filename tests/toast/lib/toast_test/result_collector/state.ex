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

defmodule ToastTest.ResultCollector.State do
  @moduledoc """
  Pure state logic for ResultCollector.

  Collects test outcomes, durations, and failures from ExUnit. Carries no
  timestamps: every time window — suite, module, and test — is derived from the
  event stream by `ToastTest.TimeWindows` and attached in `SuiteResult.build`.
  """

  defstruct [
    :times_us,
    modules: %{},
    failures: [],
    config: []
  ]

  @doc "Create initial state with the given options."
  def new(opts \\ []) do
    %__MODULE__{config: opts}
  end

  @doc "Apply an event to the state, returning updated state."
  def apply_event(state, {:module_started, %ExUnit.TestModule{name: module}}) do
    %{state | modules: Map.put_new(state.modules, module, [])}
  end

  def apply_event(state, {:test_finished, %ExUnit.Test{} = test}) do
    result = %{
      name: test.name,
      outcome: ToastTest.Formatting.test_outcome(test),
      duration_us: test.time,
      tags: %{file: test.tags[:file], line: test.tags[:line]}
    }

    modules = Map.update(state.modules, test.module, [result], &[result | &1])
    %{state | modules: modules, failures: record_failure(state.failures, test)}
  end

  def apply_event(state, {:suite_finished, times_us}) do
    %{state | times_us: times_us}
  end

  def apply_event(state, _unknown), do: state

  @doc "Convert collector state into the public test_data format."
  def to_test_data(state) do
    %{
      suite: state.config[:suite],
      times_us: state.times_us,
      modules:
        Map.new(state.modules, fn {mod, tests} -> {mod, %{tests: Enum.reverse(tests)}} end),
      failures: Enum.reverse(state.failures)
    }
  end

  defp record_failure(failures, %ExUnit.Test{state: {:failed, _}} = test), do: [test | failures]
  defp record_failure(failures, _test), do: failures
end
