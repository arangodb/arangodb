defmodule ToastTest.ServerIdMappingTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment

  describe "arango_id" do
    test "returns arango-internal ID for toast ID" do
      d = cluster_deployment()
      assert {:ok, "PRMR-abc123"} = Deployment.arango_id(d, "dbserver-0")
      assert {:ok, "PRMR-def456"} = Deployment.arango_id(d, "dbserver-1")
      assert {:ok, "CRDN-ghi789"} = Deployment.arango_id(d, "coordinator-0")
    end

    test "returns error for unknown toast ID" do
      d = cluster_deployment()
      assert {:error, :not_found} = Deployment.arango_id(d, "nonexistent")
    end

    test "returns :not_found when arango_id not set" do
      d = single_server_deployment(arango_id: nil)
      assert {:error, :not_found} = Deployment.arango_id(d, "single")
    end

    test "returns arango_id when set on single server" do
      d = single_server_deployment(arango_id: "SNGL-xyz")
      assert {:ok, "SNGL-xyz"} = Deployment.arango_id(d, "single")
    end
  end

  describe "server_by_arango_id" do
    test "returns server info for valid arango ID" do
      d = cluster_deployment()
      assert {:ok, server} = Deployment.server_by_arango_id(d, "PRMR-abc123")
      assert server.id == "dbserver-0"
      assert server.role == :dbserver
    end

    test "returns error for unknown arango ID" do
      d = cluster_deployment()
      assert {:error, :not_found} = Deployment.server_by_arango_id(d, "PRMR-unknown")
    end
  end

  # --- Helpers ---

  defp single_server_deployment(opts) do
    %Deployment{
      id: "test-single",
      controller: self(),
      servers: %{
        "single" => %Toast.Deployment.ServerInfo{
          id: "single",
          role: :single,
          port: 8529,
          endpoint: "http://localhost:8529",
          arango_id: Keyword.get(opts, :arango_id)
        }
      }
    }
  end

  defp cluster_deployment do
    %Deployment{
      id: "test-cluster",
      controller: self(),
      servers: %{
        "dbserver-0" => %Toast.Deployment.ServerInfo{
          id: "dbserver-0",
          role: :dbserver,
          port: 8529,
          endpoint: "http://localhost:8529",
          arango_id: "PRMR-abc123"
        },
        "dbserver-1" => %Toast.Deployment.ServerInfo{
          id: "dbserver-1",
          role: :dbserver,
          port: 8530,
          endpoint: "http://localhost:8530",
          arango_id: "PRMR-def456"
        },
        "coordinator-0" => %Toast.Deployment.ServerInfo{
          id: "coordinator-0",
          role: :coordinator,
          port: 8531,
          endpoint: "http://localhost:8531",
          arango_id: "CRDN-ghi789"
        }
      }
    }
  end
end
