defmodule Toast.Deployment.ClientTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment
  alias Toast.Deployment.ServerInfo

  defp server_info(id, opts) do
    %ServerInfo{
      id: id,
      role: Keyword.get(opts, :role, :single),
      port: Keyword.get(opts, :port, 8529),
      endpoint: Keyword.get(opts, :endpoint, "http://localhost:8529")
    }
  end

  defp deployment(servers, opts \\ []) do
    server_map = Map.new(servers, &{&1.id, &1})

    %Deployment{
      id: "test",
      controller: self(),
      api_version: Keyword.get(opts, :api_version),
      servers: server_map
    }
  end

  describe "client/2 with server_id (string)" do
    test "returns a client targeting that server's endpoint" do
      srv = server_info("s1", endpoint: "http://localhost:9090")
      assert {:ok, client} = Deployment.client(deployment([srv]), "s1")
      assert client.base_url == "http://localhost:9090"
    end

    test "returns {:error, :not_found} for unknown server_id" do
      assert {:error, :not_found} = Deployment.client(deployment([]), "nonexistent")
    end

    test "returns correct client when multiple servers exist" do
      srv1 = server_info("coord-0", endpoint: "http://localhost:9001", role: :coordinator)
      srv2 = server_info("db-0", endpoint: "http://localhost:9002", role: :dbserver)
      d = deployment([srv1, srv2])

      assert {:ok, client} = Deployment.client(d, "db-0")
      assert client.base_url == "http://localhost:9002"
    end

    test "applies api_version from deployment" do
      srv = server_info("s1", endpoint: "http://localhost:9090")
      d = deployment([srv], api_version: 2)

      assert {:ok, client} = Deployment.client(d, "s1")
      assert client.api_version == 2
    end
  end

  describe "client_for_role/3" do
    test "returns client for first server of the given role" do
      srv1 = server_info("coord-0", endpoint: "http://localhost:9001", role: :coordinator)
      srv2 = server_info("coord-1", endpoint: "http://localhost:9002", role: :coordinator)
      srv3 = server_info("db-0", endpoint: "http://localhost:9003", role: :dbserver)
      d = deployment([srv1, srv2, srv3])

      assert {:ok, client} = Deployment.client_for_role(d, :coordinator)
      assert client.base_url in ["http://localhost:9001", "http://localhost:9002"]
    end

    test "index targets specific server" do
      srv1 = server_info("coord-0", endpoint: "http://localhost:9001", role: :coordinator)
      srv2 = server_info("coord-1", endpoint: "http://localhost:9002", role: :coordinator)
      d = deployment([srv1, srv2])

      assert {:ok, client} = Deployment.client_for_role(d, :coordinator, 1)
      assert client.base_url in ["http://localhost:9001", "http://localhost:9002"]
    end

    test "returns {:error, :not_found} when no servers match role" do
      srv = server_info("db-0", endpoint: "http://localhost:9001", role: :dbserver)
      d = deployment([srv])

      assert {:error, :not_found} = Deployment.client_for_role(d, :coordinator)
    end

    test "returns {:error, :not_found} when index out of range" do
      srv = server_info("coord-0", endpoint: "http://localhost:9001", role: :coordinator)
      d = deployment([srv])

      assert {:error, :not_found} = Deployment.client_for_role(d, :coordinator, 5)
    end
  end
end
