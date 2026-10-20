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

defmodule ToastTest.CrashMonitor do
  @moduledoc "Aborts the test run when a server crashes unexpectedly."

  @spec handle_crash(String.t(), Toast.Process.CrashInfo.t()) :: :ok
  def handle_crash(server_id, %Toast.Process.CrashInfo{signal: signal, exit_status: exit_status}) do
    message =
      [
        "Server crashed: #{server_id}",
        if(signal, do: "(signal: #{signal})"),
        if(exit_status, do: "exit_status=#{exit_status}")
      ]
      |> Toast.Utils.compact_join(" ")

    ToastTest.Abort.abort!({:crash, message})
    ToastTest.Abort.kill_test_pid()
  end
end
