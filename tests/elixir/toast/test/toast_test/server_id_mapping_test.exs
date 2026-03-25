defmodule ToastTest.ServerIdMappingTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment
  alias Toast.Deployment.{Controller, ServerInstance}

  describe "arango_id happy path" do
    setup do
      id = "arango-id-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      inject_arango_ids(ctrl, %{
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :running,
          arango_id: "PRMR-abc123"
        },
        "dbserver-1" => %ServerInstance{
          id: "dbserver-1",
          role: :dbserver,
          operational_state: :running,
          arango_id: "PRMR-def456"
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running,
          arango_id: "CRDN-ghi789"
        }
      })

      deployment = cluster_deployment(ctrl)

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      %{ctrl: ctrl, deployment: deployment}
    end

    test "arango_id returns arango-internal ID for toast ID", %{deployment: d} do
      assert {:ok, "PRMR-abc123"} = Deployment.arango_id(d, "dbserver-0")
      assert {:ok, "PRMR-def456"} = Deployment.arango_id(d, "dbserver-1")
      assert {:ok, "CRDN-ghi789"} = Deployment.arango_id(d, "coordinator-0")
    end

    test "arango_id returns error for unknown toast ID", %{deployment: d} do
      assert {:error, :not_found} = Deployment.arango_id(d, "nonexistent")
    end
  end

  describe "server_by_arango_id happy path" do
    setup do
      id = "server-by-aid-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      inject_arango_ids(ctrl, %{
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :running,
          arango_id: "PRMR-abc123"
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running,
          arango_id: "CRDN-ghi789"
        }
      })

      deployment = cluster_deployment(ctrl)

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      %{ctrl: ctrl, deployment: deployment}
    end

    test "server_by_arango_id returns server info for valid arango ID", %{deployment: d} do
      assert {:ok, server} = Deployment.server_by_arango_id(d, "PRMR-abc123")
      assert server.id == "dbserver-0"
      assert server.role == :dbserver
    end

    test "server_by_arango_id returns error for unknown arango ID", %{deployment: d} do
      assert {:error, :not_found} = Deployment.server_by_arango_id(d, "PRMR-unknown")
    end
  end

  describe "arango_id populated after server injection" do
    test "arango_id reflects all servers in the cluster" do
      id = "arango-mapping-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(config: Toast.Deployment.Config.new(), id: id)

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      deployment = cluster_deployment(ctrl)
      assert {:error, :not_found} = Deployment.arango_id(deployment, "agent-0")

      inject_arango_ids(ctrl, %{
        "agent-0" => %ServerInstance{
          id: "agent-0",
          role: :agent,
          operational_state: :running,
          arango_id: "AGNT-001"
        },
        "agent-1" => %ServerInstance{
          id: "agent-1",
          role: :agent,
          operational_state: :running,
          arango_id: "AGNT-002"
        },
        "agent-2" => %ServerInstance{
          id: "agent-2",
          role: :agent,
          operational_state: :running,
          arango_id: "AGNT-003"
        },
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :running,
          arango_id: "PRMR-001"
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running,
          arango_id: "CRDN-001"
        }
      })

      deployment = cluster_deployment(ctrl)
      assert {:ok, "AGNT-001"} = Deployment.arango_id(deployment, "agent-0")
      assert {:ok, "PRMR-001"} = Deployment.arango_id(deployment, "dbserver-0")
      assert {:ok, "CRDN-001"} = Deployment.arango_id(deployment, "coordinator-0")

      assert {:ok, server} = Deployment.server_by_arango_id(deployment, "AGNT-002")
      assert server.id == "agent-1"
    end
  end

  describe "arango_id works for single server deployments" do
    test "returns :not_found when arango_id not set" do
      deployment = single_server_deployment_without_arango_id()
      assert {:error, :not_found} = Deployment.arango_id(deployment, "single")
    end

    test "returns arango_id when set on single server" do
      deployment = single_server_deployment_with_arango_id()
      assert {:ok, "SNGL-xyz"} = Deployment.arango_id(deployment, "single")
    end
  end

  # --- Helpers ---

  defp single_server_deployment_without_arango_id do
    %Deployment{
      id: "test-1",
      controller: self(),
      servers: %{
        "single" => %Toast.Deployment.ServerInfo{
          id: "single",
          role: :single,
          port: 8529,
          endpoint: "http://localhost:8529"
        }
      }
    }
  end

  defp single_server_deployment_with_arango_id do
    %Deployment{
      id: "test-1",
      controller: self(),
      servers: %{
        "single" => %Toast.Deployment.ServerInfo{
          id: "single",
          role: :single,
          port: 8529,
          endpoint: "http://localhost:8529",
          arango_id: "SNGL-xyz"
        }
      }
    }
  end

  defp cluster_deployment(ctrl) do
    info = Controller.get_info(ctrl)

    servers =
      info
      |> Map.get(:servers, %{})
      |> Map.new(fn {id, s} ->
        {id,
         %Toast.Deployment.ServerInfo{
           id: s.id,
           role: s.role,
           port: s.port || 8529,
           endpoint: s.endpoint || "http://localhost:8529",
           arango_id: s.arango_id
         }}
      end)

    %Deployment{
      id: "test-cluster",
      controller: ctrl,
      servers: servers
    }
  end

  defp inject_arango_ids(ctrl, servers) do
    :sys.replace_state(ctrl, fn state ->
      %{state | servers: servers, status: :ready}
    end)
  end
end
