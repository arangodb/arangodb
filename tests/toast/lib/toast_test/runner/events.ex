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
  Event emission for the test-runner layer. Every runner-side event has
  exactly one constructor here; no caller builds event maps by hand. The
  full event vocabulary is documented in `docs/events.md`.

  Test lifecycle events are broadcast to both ExUnit (for real-time
  formatting) and the EventStore (for post-mortem analysis). Use those for
  tests/modules that were actually executed. Tests that were never run
  (excluded, skipped, aborted) only need ExUnit notification for formatter
  bookkeeping — use `emit_not_executed/2` for those.

  For after-the-fact reporting (e.g., JS tests where results are parsed after
  execution), pass explicit timestamps via the `:timestamp` option.

  Diagnostics events (netstat, infrastructure issues, timeout kills) are
  recorded in the EventStore only.
  """

  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.{EventStore, Formatting}

  @type manager :: Compat.event_manager()
  @type timestamp_opt :: {:timestamp, non_neg_integer()}

  @spec suite_started(manager(), keyword()) :: :ok
  def suite_started(manager, opts) do
    EventStore.notify(%{event: :suite_started})
    Compat.suite_started(manager, opts)
  end

  @spec suite_finished(manager(), map()) :: :ok
  def suite_finished(manager, times_us) do
    Compat.suite_finished(manager, times_us)
    EventStore.notify(%{event: :suite_finished})
  end

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

  @doc """
  Mark the end of the between-tests barrier for the previous test.

  Extends the test's attribution window: crashes whose detection lands
  during the barrier wait still attribute to the test that provoked them.
  """
  @spec between_tests_finished(module(), atom()) :: :ok
  def between_tests_finished(module, name) when is_atom(module) and is_atom(name) do
    EventStore.notify(%{event: :between_tests_finished, module: module, name: name})
  end

  @doc "Emit a test that was never executed (excluded, skipped, aborted) to ExUnit only."
  @spec emit_not_executed(manager(), ExUnit.Test.t()) :: :ok
  def emit_not_executed(manager, test) do
    Compat.test_started(manager, test)
    Compat.test_finished(manager, test)
  end

  # --- Diagnostics events (EventStore only) ---

  @spec netstat_snapshot(non_neg_integer(), atom() | nil) :: :ok
  def netstat_snapshot(total, label \\ nil)
      when is_integer(total) and total >= 0 and is_atom(label) do
    EventStore.notify(%{event: :netstat_snapshot, total: total, label: label})
  end

  @spec infrastructure_issue(atom(), map()) :: :ok
  def infrastructure_issue(subtype, detail) when is_atom(subtype) and is_map(detail) do
    EventStore.notify(%{event: :infrastructure_issue, subtype: subtype, detail: detail})
  end

  @typedoc "Affected-server entry carried by `:timeout_kill` events."
  @type timeout_kill_server :: %{
          server_id: String.t(),
          os_pid: non_neg_integer() | nil,
          log_file: Path.t() | nil
        }

  @spec timeout_kill(atom(), String.t(), [timeout_kill_server()]) :: :ok
  def timeout_kill(source, reason, servers)
      when is_atom(source) and is_binary(reason) and is_list(servers) do
    EventStore.notify(%{event: :timeout_kill, source: source, reason: reason, servers: servers})
  end

  @spec agency_dump_captured(String.t(), Path.t()) :: :ok
  def agency_dump_captured(deployment_id, path)
      when is_binary(deployment_id) and is_binary(path) do
    EventStore.notify(%{event: :agency_dump_captured, deployment_id: deployment_id, path: path})
  end

  defp maybe_timestamp(event, opts) do
    case Keyword.fetch(opts, :timestamp) do
      {:ok, ts} -> Map.put(event, :timestamp, ts)
      :error -> event
    end
  end
end
