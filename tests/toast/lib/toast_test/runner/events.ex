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

defmodule ToastTest.Runner.Events do
  @moduledoc """
  Broadcasts test lifecycle events to both ExUnit (for real-time formatting)
  and the EventStore (for post-mortem analysis).

  Use this module for tests/modules that were actually executed. Tests that
  were never run (excluded, skipped, aborted) only need ExUnit notification
  for formatter bookkeeping — use `emit_not_executed/2` for those.

  For after-the-fact reporting (e.g., JS tests where results are parsed after
  execution), pass explicit timestamps via the `:timestamp` option.
  """

  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.{EventStore, Formatting}

  @type manager :: Compat.event_manager()
  @type timestamp_opt :: {:timestamp, non_neg_integer()}

  @spec module_started(manager(), module(), ExUnit.TestModule.t(), [timestamp_opt]) :: :ok
  def module_started(manager, module, test_module, opts \\ []) do
    EventStore.notify(maybe_timestamp(%{event: :module_started, module: module}, opts))
    Compat.module_started(manager, test_module)
  end

  @spec module_finished(manager(), module(), ExUnit.TestModule.t(), [timestamp_opt]) :: :ok
  def module_finished(manager, module, test_module, opts \\ []) do
    Compat.module_finished(manager, test_module)
    EventStore.notify(maybe_timestamp(%{event: :module_finished, module: module}, opts))
  end

  @spec test_started(manager(), ExUnit.Test.t(), [timestamp_opt]) :: :ok
  def test_started(manager, test, opts \\ []) do
    EventStore.notify(
      maybe_timestamp(%{event: :test_started, module: test.module, name: test.name}, opts)
    )

    Compat.test_started(manager, test)
  end

  @spec test_finished(manager(), ExUnit.Test.t(), [timestamp_opt]) :: :ok
  def test_finished(manager, test, opts \\ []) do
    Compat.test_finished(manager, test)

    EventStore.notify(
      maybe_timestamp(
        %{
          event: :test_finished,
          module: test.module,
          name: test.name,
          outcome: Formatting.test_outcome(test),
          duration_us: test.time
        },
        opts
      )
    )
  end

  @doc "Emit a test that was never executed (excluded, skipped, aborted) to ExUnit only."
  @spec emit_not_executed(manager(), ExUnit.Test.t()) :: :ok
  def emit_not_executed(manager, test) do
    Compat.test_started(manager, test)
    Compat.test_finished(manager, test)
  end

  defp maybe_timestamp(event, opts) do
    case Keyword.fetch(opts, :timestamp) do
      {:ok, ts} -> Map.put(event, :timestamp, ts)
      :error -> event
    end
  end
end
