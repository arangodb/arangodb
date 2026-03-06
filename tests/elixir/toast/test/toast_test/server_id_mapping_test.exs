defmodule ToastTest.ServerIdMappingTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment
  alias Toast.Deployment.{Controller, ServerInstance}

  test "cluster_id returns error for single server deployment" do
    deployment = single_server_deployment(self())
    assert {:error, :not_cluster} = Deployment.cluster_id(deployment, "server-0")
  end

  test "server_by_cluster_id returns error for single server deployment" do
    deployment = single_server_deployment(self())
    assert {:error, :not_cluster} = Deployment.server_by_cluster_id(deployment, "PRMR-abc")
  end

  describe "cluster_id happy path" do
    setup do
      {:ok, ctrl} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      inject_cluster_state(ctrl, %{
        servers: %{
          "dbserver-0" => %ServerInstance{
            id: "dbserver-0",
            role: :dbserver,
            operational_state: :running
          },
          "dbserver-1" => %ServerInstance{
            id: "dbserver-1",
            role: :dbserver,
            operational_state: :running
          },
          "coordinator-0" => %ServerInstance{
            id: "coordinator-0",
            role: :coordinator,
            operational_state: :running
          }
        },
        cluster_id_mapping: %{
          "dbserver-0" => "PRMR-abc123",
          "dbserver-1" => "PRMR-def456",
          "coordinator-0" => "CRDN-ghi789"
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

    test "cluster_id returns cluster-internal ID for toast ID", %{deployment: d} do
      assert {:ok, "PRMR-abc123"} = Deployment.cluster_id(d, "dbserver-0")
      assert {:ok, "PRMR-def456"} = Deployment.cluster_id(d, "dbserver-1")
      assert {:ok, "CRDN-ghi789"} = Deployment.cluster_id(d, "coordinator-0")
    end

    test "cluster_id returns error for unknown toast ID", %{deployment: d} do
      assert {:error, :not_found} = Deployment.cluster_id(d, "nonexistent")
    end
  end

  describe "server_by_cluster_id happy path" do
    setup do
      {:ok, ctrl} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      inject_cluster_state(ctrl, %{
        servers: %{
          "dbserver-0" => %ServerInstance{
            id: "dbserver-0",
            role: :dbserver,
            operational_state: :running
          },
          "coordinator-0" => %ServerInstance{
            id: "coordinator-0",
            role: :coordinator,
            operational_state: :running
          }
        },
        cluster_id_mapping: %{
          "dbserver-0" => "PRMR-abc123",
          "coordinator-0" => "CRDN-ghi789"
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

    test "server_by_cluster_id returns server info for valid cluster ID", %{deployment: d} do
      assert {:ok, server} = Deployment.server_by_cluster_id(d, "PRMR-abc123")
      assert server.id == "dbserver-0"
      assert server.role == :dbserver
    end

    test "server_by_cluster_id returns error for unknown cluster ID", %{deployment: d} do
      assert {:error, :not_found} = Deployment.server_by_cluster_id(d, "PRMR-unknown")
    end
  end

  describe "cluster_id_mapping populated after server injection" do
    test "mapping reflects all servers in the cluster" do
      {:ok, ctrl} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      deployment = cluster_deployment(ctrl)
      assert {:error, :not_found} = Deployment.cluster_id(deployment, "agent-0")

      inject_cluster_state(ctrl, %{
        servers: %{
          "agent-0" => %ServerInstance{id: "agent-0", role: :agent, operational_state: :running},
          "agent-1" => %ServerInstance{id: "agent-1", role: :agent, operational_state: :running},
          "agent-2" => %ServerInstance{id: "agent-2", role: :agent, operational_state: :running},
          "dbserver-0" => %ServerInstance{
            id: "dbserver-0",
            role: :dbserver,
            operational_state: :running
          },
          "coordinator-0" => %ServerInstance{
            id: "coordinator-0",
            role: :coordinator,
            operational_state: :running
          }
        },
        cluster_id_mapping: %{
          "agent-0" => "AGNT-001",
          "agent-1" => "AGNT-002",
          "agent-2" => "AGNT-003",
          "dbserver-0" => "PRMR-001",
          "coordinator-0" => "CRDN-001"
        }
      })

      assert {:ok, "AGNT-001"} = Deployment.cluster_id(deployment, "agent-0")
      assert {:ok, "PRMR-001"} = Deployment.cluster_id(deployment, "dbserver-0")
      assert {:ok, "CRDN-001"} = Deployment.cluster_id(deployment, "coordinator-0")

      assert {:ok, server} = Deployment.server_by_cluster_id(deployment, "AGNT-002")
      assert server.id == "agent-1"
    end
  end

  # --- Helpers ---

  defp single_server_deployment(ctrl) do
    %Deployment{
      id: "test-1",
      mode: :single_server,
      config: Toast.Config.load(),
      controller: ctrl,
      endpoint: "http://localhost:8529"
    }
  end

  defp cluster_deployment(ctrl) do
    %Deployment{
      id: "test-cluster",
      mode: :cluster,
      config: Toast.Config.load(),
      controller: ctrl,
      endpoint: "http://localhost:8529"
    }
  end

  defp inject_cluster_state(ctrl, %{servers: servers, cluster_id_mapping: mapping}) do
    :sys.replace_state(ctrl, fn state ->
      agents = for {id, s} <- servers, s.role == :agent, do: id
      dbservers = for {id, s} <- servers, s.role == :dbserver, do: id
      coordinators = for {id, s} <- servers, s.role == :coordinator, do: id

      mode_state = %{
        state.mode_state
        | agents: agents,
          dbservers: dbservers,
          coordinators: coordinators,
          cluster_id_mapping: mapping
      }

      %{state | servers: servers, mode_state: mode_state, status: :ready}
    end)
  end
end
