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

defmodule ToastTest.Runner.FailureFormatter do
  @moduledoc false

  @runner_modules [
    ExUnit.Runner,
    ToastTest.Runner,
    ToastTest.Runner.TestExecution,
    ToastTest.Runner.TestProcess
  ]

  @spec prune_stacktrace(Exception.stacktrace()) :: Exception.stacktrace()
  def prune_stacktrace([{ExUnit.Assertions, _, _, _} | t]), do: prune_stacktrace(t)
  def prune_stacktrace([{mod, _, _, _} | _]) when mod in @runner_modules, do: []
  def prune_stacktrace([h | t]), do: [h | prune_stacktrace(t)]
  def prune_stacktrace([]), do: []

  @spec failed(atom() | {:EXIT, pid()}, term(), Exception.stacktrace()) :: {:failed, list()}
  def failed(:error, %ExUnit.MultiError{errors: errors}, _stack) do
    errors =
      Enum.map(errors, fn {kind, reason, stack} ->
        {kind, Exception.normalize(kind, reason, stack), prune_stacktrace(stack)}
      end)

    {:failed, errors}
  end

  def failed(kind, reason, stack) do
    {:failed, [{kind, Exception.normalize(kind, reason, stack), stack}]}
  end
end
