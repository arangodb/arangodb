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

defmodule Toast.Deployment.ClientTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment
  alias Toast.Deployment.ServerInfo

  defp server_info(id, opts) do
    %ServerInfo{
      id: id,
      role: Keyword.get(opts, :role, :single),
      port: Keyword.get(opts, :port, 8529),
      endpoint: Keyword.get(opts, :endpoint, "http://localhost:8529"),
      arango_id: Keyword.get(opts, :arango_id)
    }
  end

  defp deployment(servers, opts \\ []) do
    server_map = Map.new(servers, &{&1.id, &1})

    %Deployment{
      id: "test",
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

  describe "client_for_arango_id/2" do
    test "returns client for server matching arango_id" do
      srv =
        server_info("db-0",
          endpoint: "http://localhost:9001",
          role: :dbserver,
          arango_id: "PRMR-abc123"
        )

      d = deployment([srv])

      assert {:ok, client} = Deployment.client_for_arango_id(d, "PRMR-abc123")
      assert client.base_url == "http://localhost:9001"
    end

    test "returns {:error, :not_found} for unknown arango_id" do
      srv =
        server_info("db-0",
          endpoint: "http://localhost:9001",
          role: :dbserver,
          arango_id: "PRMR-abc123"
        )

      d = deployment([srv])

      assert {:error, :not_found} = Deployment.client_for_arango_id(d, "PRMR-unknown")
    end

    test "returns {:error, :not_found} when no servers have arango_ids" do
      srv = server_info("db-0", endpoint: "http://localhost:9001", role: :dbserver)
      d = deployment([srv])

      assert {:error, :not_found} = Deployment.client_for_arango_id(d, "PRMR-abc123")
    end

    test "applies api_version from deployment" do
      srv =
        server_info("db-0",
          endpoint: "http://localhost:9001",
          role: :dbserver,
          arango_id: "PRMR-abc123"
        )

      d = deployment([srv], api_version: 2)

      assert {:ok, client} = Deployment.client_for_arango_id(d, "PRMR-abc123")
      assert client.api_version == 2
    end

    test "selects correct server among multiple" do
      srv1 =
        server_info("db-0",
          endpoint: "http://localhost:9001",
          role: :dbserver,
          arango_id: "PRMR-aaa"
        )

      srv2 =
        server_info("db-1",
          endpoint: "http://localhost:9002",
          role: :dbserver,
          arango_id: "PRMR-bbb"
        )

      d = deployment([srv1, srv2])

      assert {:ok, client} = Deployment.client_for_arango_id(d, "PRMR-bbb")
      assert client.base_url == "http://localhost:9002"
    end
  end

  describe "client_for_arango_id!/2" do
    test "returns client for matching arango_id" do
      srv =
        server_info("db-0",
          endpoint: "http://localhost:9001",
          role: :dbserver,
          arango_id: "PRMR-abc123"
        )

      d = deployment([srv])

      client = Deployment.client_for_arango_id!(d, "PRMR-abc123")
      assert client.base_url == "http://localhost:9001"
    end

    test "raises ArgumentError for unknown arango_id" do
      d = deployment([])

      assert_raise ArgumentError, ~r/no server with arango_id/, fn ->
        Deployment.client_for_arango_id!(d, "PRMR-unknown")
      end
    end
  end

  describe "cluster?/1" do
    test "returns false for single server deployment" do
      srv = server_info("single", role: :single)
      refute Deployment.cluster?(deployment([srv]))
    end

    test "returns true when servers include coordinator" do
      srv = server_info("coord-0", role: :coordinator)
      assert Deployment.cluster?(deployment([srv]))
    end

    test "returns true when servers include dbserver" do
      srv = server_info("db-0", role: :dbserver)
      assert Deployment.cluster?(deployment([srv]))
    end

    test "returns true when servers include agent" do
      srv = server_info("agent-0", role: :agent)
      assert Deployment.cluster?(deployment([srv]))
    end

    test "returns false for empty deployment" do
      refute Deployment.cluster?(deployment([]))
    end
  end

  describe "default_endpoint/1" do
    test "returns single server endpoint" do
      srv = server_info("single", endpoint: "http://localhost:8529", role: :single)
      assert Deployment.default_endpoint(deployment([srv])) == "http://localhost:8529"
    end

    test "returns first coordinator endpoint ordered by ID" do
      srv1 =
        server_info("coord-1", endpoint: "http://localhost:9002", role: :coordinator)

      srv2 =
        server_info("coord-0", endpoint: "http://localhost:9001", role: :coordinator)

      assert Deployment.default_endpoint(deployment([srv1, srv2])) == "http://localhost:9001"
    end

    test "ignores dbserver and agent roles" do
      db = server_info("db-0", endpoint: "http://localhost:9001", role: :dbserver)
      agent = server_info("agent-0", endpoint: "http://localhost:9002", role: :agent)
      coord = server_info("coord-0", endpoint: "http://localhost:9003", role: :coordinator)

      assert Deployment.default_endpoint(deployment([db, agent, coord])) ==
               "http://localhost:9003"
    end

    test "returns nil when no coordinator or single server exists" do
      db = server_info("db-0", endpoint: "http://localhost:9001", role: :dbserver)
      assert Deployment.default_endpoint(deployment([db])) == nil
    end

    test "returns nil for empty deployment" do
      assert Deployment.default_endpoint(deployment([])) == nil
    end
  end
end
