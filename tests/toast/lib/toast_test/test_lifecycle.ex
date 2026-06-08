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

defmodule ToastTest.TestLifecycle do
  @moduledoc false
  # Shared test lifecycle primitives used by both Interactive and Runner.

  alias ToastTest.ExUnitCompat, as: Compat

  @doc false
  @spec spawn_setup_all(module(), map()) :: {pid(), reference()}
  def spawn_setup_all(module, context) do
    parent_pid = self()

    spawn_monitor(fn ->
      Compat.register_on_exit(self())

      result =
        try do
          {:ok, Compat.get_setup_all(module, context)}
        catch
          kind, error ->
            {:error, {kind, error, __STACKTRACE__}}
        end

      send(parent_pid, {self(), :setup_all, result})

      ref = Process.monitor(parent_pid)

      receive do
        {^parent_pid, :exit} -> :ok
        {:DOWN, ^ref, _, _, _} -> :ok
      end
    end)
  end

  @doc false
  @spec exit_setup_all(pid(), reference()) :: :ok
  def exit_setup_all(pid, ref) do
    send(pid, {self(), :exit})

    receive do
      {:DOWN, ^ref, _, _, _} -> :ok
    end
  end

  @doc false
  @spec run_on_exit(pid(), timeout()) :: :ok | {atom(), term(), Exception.stacktrace()}
  def run_on_exit(pid, timeout) do
    Compat.run_on_exit(pid, timeout)
  end
end
