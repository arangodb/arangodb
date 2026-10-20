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

defmodule Toast.Deployment.HealthBarrierTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment
  alias Toast.Deployment.{Config, Controller, HealthBarrier, ServerInstance}

  defp start_deployment(servers, status \\ :ready) do
    id = "health-barrier-test-#{System.unique_integer([:positive])}"
    server_map = Map.new(servers, fn server -> {server.id, server} end)

    {:ok, ctrl} =
      Controller.start_link(
        config: Config.new(),
        id: id,
        servers: server_map,
        status: status
      )

    on_exit(fn ->
      try do
        GenServer.stop(ctrl)
      catch
        :exit, _ -> :ok
      end
    end)

    %Deployment{id: id, controller: ctrl}
  end

  defp server(id, opts \\ []) do
    defaults = [
      id: id,
      role: :single,
      operational_state: :running,
      pid: 12_345,
      health_monitor: :fake_hm,
      expecting_exit: false
    ]

    struct!(ServerInstance, Keyword.merge(defaults, opts))
  end

  describe "await_healthy/2" do
    test "returns :ok when all servers report healthy" do
      deployment = start_deployment([server("a"), server("b")])
      hm = fn _ -> :healthy end

      assert HealthBarrier.await_healthy(deployment, hm_probe: hm, timeout: 1_000) == :ok
    end

    test "servers without a health monitor are skipped via :not_monitored" do
      deployment = start_deployment([server("a", health_monitor: nil)])
      hm = fn %{health_monitor: nil} -> :not_monitored end

      assert HealthBarrier.await_healthy(deployment, hm_probe: hm, timeout: 1_000) == :ok
    end

    test "returns :ok fast-path when controller already reports :failed" do
      deployment = start_deployment([server("a")], :failed)
      hm = fn _ -> flunk("hm_probe should not be called when controller is already :failed") end

      assert HealthBarrier.await_healthy(deployment, hm_probe: hm, timeout: 1_000) == :ok
    end

    test "servers with operational_state != :running are skipped" do
      deployment =
        start_deployment([
          server("a", operational_state: :stopped),
          server("b", operational_state: :crashed),
          server("c", operational_state: :paused)
        ])

      hm = fn _ -> flunk("hm_probe should not be called for non-running servers") end

      assert HealthBarrier.await_healthy(deployment, hm_probe: hm, timeout: 1_000) == :ok
    end

    test "suspended HM fails fast with a descriptive error" do
      deployment = start_deployment([server("a")])
      hm = fn _ -> :suspended end

      assert {:error, message} =
               HealthBarrier.await_healthy(deployment, hm_probe: hm, timeout: 5_000)

      assert message =~ "Server a"
      assert message =~ "suspended"
    end

    test "unhealthy HM fails fast with a descriptive error" do
      deployment = start_deployment([server("a")])
      hm = fn _ -> :unhealthy end

      assert {:error, message} =
               HealthBarrier.await_healthy(deployment, hm_probe: hm, timeout: 5_000)

      assert message =~ "Server a"
      assert message =~ "unresponsive"
    end

    test "failing HM that recovers returns :ok" do
      deployment = start_deployment([server("a")])

      {:ok, agent} = Agent.start_link(fn -> [:failing, :failing, :healthy] end)

      hm = fn _ ->
        Agent.get_and_update(agent, fn
          [next | rest] -> {next, rest}
          [] -> {:healthy, []}
        end)
      end

      assert HealthBarrier.await_healthy(deployment,
               hm_probe: hm,
               timeout: 5_000,
               poll_interval: 10
             ) == :ok
    end

    test "failing HM that never recovers times out" do
      deployment = start_deployment([server("a")])
      hm = fn _ -> :failing end

      assert {:error, message} =
               HealthBarrier.await_healthy(deployment,
                 hm_probe: hm,
                 timeout: 200,
                 poll_interval: 10
               )

      assert message =~ "Server a"
      assert message =~ "did not recover"
    end

    test "failing HM that tips to :unhealthy during poll returns an unresponsive error" do
      deployment = start_deployment([server("a")])

      {:ok, agent} = Agent.start_link(fn -> [:failing, :failing, :unhealthy] end)

      hm = fn _ ->
        Agent.get_and_update(agent, fn
          [next | rest] -> {next, rest}
          [] -> {:unhealthy, []}
        end)
      end

      assert {:error, message} =
               HealthBarrier.await_healthy(deployment,
                 hm_probe: hm,
                 timeout: 5_000,
                 poll_interval: 10
               )

      assert message =~ "unresponsive"
    end
  end
end
