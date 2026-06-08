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

defmodule Toast.Deployment.ResolveTargetTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.{Controller, ServerInstance}

  # --- ClusterController target resolution ---

  describe "Controller (cluster) string target" do
    setup [:start_cluster_with_servers]

    test "known server_id resolves successfully", %{ctrl: ctrl} do
      result = Controller.stop_server(ctrl, "dbserver-0")
      assert {:error, {:unexpected_state, _}} = result
    end

    test "unknown server_id returns :not_found", %{ctrl: ctrl} do
      assert {:error, :not_found} = Controller.stop_server(ctrl, "nonexistent")
    end
  end

  describe "Controller (cluster) role target" do
    setup [:start_cluster_with_servers]

    test "role: :dbserver resolves all dbservers", %{ctrl: ctrl} do
      set_operational_state(ctrl, "dbserver-0", :paused)
      set_operational_state(ctrl, "dbserver-1", :paused)
      set_operational_state(ctrl, "dbserver-2", :paused)

      result = Controller.stop_server(ctrl, role: :dbserver)
      assert {:error, {:unexpected_state, :paused}} = result
    end

    test "unknown role returns :no_servers_for_role", %{ctrl: ctrl} do
      assert {:error, {:no_servers_for_role, :agent}} =
               Controller.stop_server(ctrl, role: :agent)
    end
  end

  describe "Controller (cluster) role+index target" do
    setup [:start_cluster_with_servers]

    test "role+index resolves single server", %{ctrl: ctrl} do
      set_operational_state(ctrl, "coordinator-0", :paused)

      result = Controller.stop_server(ctrl, role: :coordinator, index: 0)
      assert {:error, {:unexpected_state, :paused}} = result
    end

    test "out-of-bounds index returns :no_server_at_index", %{ctrl: ctrl} do
      result = Controller.stop_server(ctrl, role: :coordinator, index: 5)
      assert {:error, {:no_server_at_index, :coordinator, 5}} = result
    end
  end

  describe "Controller (cluster) arango_id target" do
    setup [:start_cluster_with_servers]

    test "known arango_id resolves to correct server", %{ctrl: ctrl} do
      inject_arango_ids(ctrl, %{
        "dbserver-0" => "PRMR-abc123",
        "dbserver-1" => "PRMR-def456"
      })

      set_operational_state(ctrl, "dbserver-0", :paused)

      result = Controller.stop_server(ctrl, arango_id: "PRMR-abc123")
      assert {:error, {:unexpected_state, :paused}} = result
    end

    test "unknown arango_id returns :not_found", %{ctrl: ctrl} do
      inject_arango_ids(ctrl, %{"dbserver-0" => "PRMR-abc123"})

      result = Controller.stop_server(ctrl, arango_id: "PRMR-unknown")
      assert {:error, :not_found} = result
    end
  end

  describe "Controller (cluster) invalid target" do
    setup [:start_cluster_with_servers]

    test "invalid target format returns :invalid_target", %{ctrl: ctrl} do
      result = Controller.stop_server(ctrl, 12_345)
      assert {:error, {:invalid_target, 12_345}} = result
    end

    test "empty keyword list returns :invalid_target", %{ctrl: ctrl} do
      result = Controller.stop_server(ctrl, [])
      assert {:error, {:invalid_target, []}} = result
    end
  end

  # --- SingleServerController target resolution ---

  describe "Controller (single server) string target" do
    test "matching server_id resolves successfully" do
      id = "ssc-resolve-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => %ServerInstance{id: id, role: :single}}
        )

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      result = Controller.stop_server(ctrl, id)
      assert {:error, {:unexpected_state, _}} = result
    end

    test "non-matching server_id returns :not_found" do
      id = "ssc-resolve-#{System.unique_integer([:positive])}"

      {:ok, ctrl} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :not_found} = Controller.stop_server(ctrl, "wrong-id")
    end
  end

  describe "Controller (single server) role target" do
    test "role: :single resolves to the server" do
      id = "ssc-role-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => %ServerInstance{id: id, role: :single}}
        )

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      result = Controller.stop_server(ctrl, role: :single)
      assert {:error, {:unexpected_state, _}} = result
    end

    test "other roles return :no_servers_for_role" do
      id = "ssc-role-#{System.unique_integer([:positive])}"

      {:ok, ctrl} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, {:no_servers_for_role, :dbserver}} =
               Controller.stop_server(ctrl, role: :dbserver)

      assert {:error, {:no_servers_for_role, :coordinator}} =
               Controller.stop_server(ctrl, role: :coordinator)

      assert {:error, {:no_servers_for_role, :agent}} =
               Controller.stop_server(ctrl, role: :agent)
    end
  end

  describe "Controller (single server) invalid target" do
    test "invalid target format returns :invalid_target" do
      id = "ssc-invalid-#{System.unique_integer([:positive])}"

      {:ok, ctrl} = Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, {:invalid_target, 42}} = Controller.stop_server(ctrl, 42)
    end
  end

  # --- Through Deployment public API ---

  describe "Deployment.stop_server target resolution" do
    test "cluster deployment with role target" do
      id = "cluster-resolve-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{
            "dbserver-0" => %ServerInstance{
              id: "dbserver-0",
              role: :dbserver,
              operational_state: :paused
            },
            "coordinator-0" => %ServerInstance{
              id: "coordinator-0",
              role: :coordinator,
              operational_state: :running
            }
          },
          status: :ready
        )

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      deployment = cluster_deployment(ctrl)

      result = Toast.Deployment.stop_server(deployment, role: :dbserver)
      assert {:error, {:unexpected_state, :paused}} = result
    end

    test "single server deployment with role: :single" do
      id = "ssc-deploy-api-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => %ServerInstance{id: id, role: :single}}
        )

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      deployment = %Toast.Deployment{
        id: id,
        controller: ctrl
      }

      result = Toast.Deployment.stop_server(deployment, role: :single)
      assert {:error, {:unexpected_state, _}} = result
    end
  end

  # --- Setup helpers ---

  defp start_cluster_with_servers(_context) do
    id = "cluster-#{System.unique_integer([:positive])}"

    servers = %{
      "dbserver-0" => %ServerInstance{id: "dbserver-0", role: :dbserver},
      "dbserver-1" => %ServerInstance{id: "dbserver-1", role: :dbserver},
      "dbserver-2" => %ServerInstance{id: "dbserver-2", role: :dbserver},
      "coordinator-0" => %ServerInstance{id: "coordinator-0", role: :coordinator}
    }

    {:ok, ctrl} =
      Controller.start_link(
        config: Toast.Deployment.Config.new(),
        id: id,
        servers: servers,
        status: :ready
      )

    on_exit(fn ->
      try do
        GenServer.stop(ctrl)
      catch
        :exit, _ -> :ok
      end
    end)

    %{ctrl: ctrl}
  end

  defp inject_arango_ids(ctrl, mapping) do
    :sys.replace_state(ctrl, fn state ->
      servers =
        Map.new(state.servers, fn {id, server} ->
          arango_id = Map.get(mapping, id)
          {id, %{server | arango_id: arango_id}}
        end)

      %{state | servers: servers}
    end)
  end

  defp set_operational_state(ctrl, server_id, op_state) do
    :sys.replace_state(ctrl, fn state ->
      server = state.servers[server_id]
      updated = %{server | operational_state: op_state}
      %{state | servers: Map.put(state.servers, server_id, updated)}
    end)
  end

  defp cluster_deployment(ctrl) do
    %Toast.Deployment{
      id: "test-resolve",
      controller: ctrl
    }
  end
end
