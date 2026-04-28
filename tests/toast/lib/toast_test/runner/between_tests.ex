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

defmodule ToastTest.Runner.BetweenTests do
  @moduledoc """
  Default between-tests health check.

  Runs after each test when the suite does not define `between_tests/2`.
  Verifies the deployment is still ready before the next test starts.
  """

  alias Toast.Deployment

  @spec check(Deployment.t(), ExUnit.Test.t()) :: :ok | {:error, String.t()}
  def check(%Deployment{} = deployment, prev_test) do
    case Deployment.status(deployment) do
      :ready ->
        :ok

      :degraded ->
        {:error, format_degraded_message(deployment, prev_test)}

      :failed ->
        {:error, format_crash_message(Deployment.deployment_error(deployment))}

      other ->
        {:error, "Deployment not ready (status: #{other})"}
    end
  end

  defp format_crash_message(nil), do: "Deployment failed (no crash details available)"

  defp format_crash_message({:server_crashed, server_id, crash_info}) do
    "Server crashed (#{server_id}) #{format_crash_exit(crash_info)}"
  end

  defp format_crash_message({:server_unhealthy, server_id}) do
    "Server became unresponsive (#{server_id})"
  end

  defp format_crash_exit(ci) do
    signal_part = if ci.signal, do: " signal=#{ci.signal}", else: ""
    "exit_status=#{ci.exit_status}#{signal_part}"
  end

  defp format_degraded_message(deployment, prev_test) do
    downed =
      deployment
      |> Deployment.server_instances()
      |> Enum.filter(&(&1.operational_state in [:stopped, :killed, :paused]))

    names = Enum.map_join(downed, ", ", & &1.id)
    test_context = format_test_context(prev_test)

    "Deployment is degraded#{test_context} -- " <>
      "servers [#{names}] are still down. " <>
      "Tests must restore all servers before finishing."
  end

  defp format_test_context(nil), do: ""
  defp format_test_context(%{name: name}), do: " after test \"#{name}\""
  defp format_test_context(_), do: ""
end
