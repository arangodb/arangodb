defmodule Toast.Client.GraphTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "list/1" do
    test "sends GET to /_api/gharial and unwraps graphs" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/gharial"

        send_encoded_response(conn, 200, %{"graphs" => [%{"_key" => "mygraph"}]})
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"_key" => "mygraph"}]} = Client.Graph.list(client)
    end
  end

  describe "create/3" do
    test "sends POST to /_api/gharial with name and edge definitions" do
      edge_defs = [%{collection: "edges", from: ["v1"], to: ["v2"]}]

      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/gharial"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "mygraph"
        assert is_list(decoded["edgeDefinitions"])

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "mygraph"}})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"_key" => "mygraph"}} = Client.Graph.create(client, "mygraph", edge_defs)
    end
  end

  describe "create/4" do
    test "translates opts to camelCase body fields" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["orphanCollections"] == ["orphan"]
        assert decoded["isSmart"] == true

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "g"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Graph.create(client, "g", [],
                 orphan_collections: ["orphan"],
                 is_smart: true
               )
    end

    test "passes options through as-is" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["options"] == %{"replicationFactor" => 3}

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "g"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Graph.create(client, "g", [], options: %{"replicationFactor" => 3})
    end
  end

  describe "get/2" do
    test "sends GET to /_api/gharial/{name} and unwraps graph" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/gharial/mygraph"

        send_encoded_response(conn, 200, %{"graph" => %{"_key" => "mygraph"}})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"_key" => "mygraph"}} = Client.Graph.get(client, "mygraph")
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/gharial/{name}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/gharial/mygraph"

        send_encoded_response(conn, 202, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Graph.drop(client, "mygraph")
    end

    test "404 is treated as success (idempotent)" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.Graph.drop(client, "nonexistent")
    end
  end

  describe "drop/3" do
    test "passes dropCollections query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["dropCollections"] == "true"
        send_encoded_response(conn, 202, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Graph.drop(client, "g", drop_collections: true)
    end
  end

  describe "add_vertex_collection/3" do
    test "sends POST to /_api/gharial/{name}/vertex" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/gharial/mygraph/vertex"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["collection"] == "vertices"

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "mygraph"}})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"_key" => "mygraph"}} =
               Client.Graph.add_vertex_collection(client, "mygraph", "vertices")
    end
  end

  describe "remove_vertex_collection/3" do
    test "sends DELETE to /_api/gharial/{name}/vertex/{coll}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/gharial/mygraph/vertex/vertices"

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "mygraph"}})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"_key" => "mygraph"}} =
               Client.Graph.remove_vertex_collection(client, "mygraph", "vertices")
    end
  end

  describe "remove_vertex_collection/4" do
    test "passes dropCollection (singular) query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["dropCollection"] == "true"

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "g"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Graph.remove_vertex_collection(client, "g", "v", drop_collection: true)
    end
  end

  describe "add_edge_definition/3" do
    test "sends POST to /_api/gharial/{name}/edge" do
      definition = %{collection: "edges", from: ["v1"], to: ["v2"]}

      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/gharial/mygraph/edge"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["collection"] == "edges"

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "mygraph"}})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"_key" => "mygraph"}} =
               Client.Graph.add_edge_definition(client, "mygraph", definition)
    end
  end

  describe "replace_edge_definition/4" do
    test "sends PUT to /_api/gharial/{name}/edge/{coll}" do
      definition = %{collection: "edges", from: ["v1", "v3"], to: ["v2"]}

      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/gharial/mygraph/edge/edges"

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "mygraph"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Graph.replace_edge_definition(client, "mygraph", "edges", definition)
    end
  end

  describe "remove_edge_definition/3" do
    test "sends DELETE to /_api/gharial/{name}/edge/{coll}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/gharial/mygraph/edge/edges"

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "mygraph"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} = Client.Graph.remove_edge_definition(client, "mygraph", "edges")
    end
  end

  describe "remove_edge_definition/4" do
    test "passes dropCollections (plural) query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["dropCollections"] == "true"

        send_encoded_response(conn, 202, %{"graph" => %{"_key" => "g"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Graph.remove_edge_definition(client, "g", "e", drop_collections: true)
    end
  end

  describe "bang variants" do
    test "list! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"graphs" => []})
      end

      client = client_with_plug(plug)
      assert [] = Client.Graph.list!(client)
    end

    test "create! raises on error" do
      client = client_with_plug(json_plug(500, %{"error" => true}))

      assert_raise RuntimeError, ~r/Graph\.create failed/, fn ->
        Client.Graph.create!(client, "g", [])
      end
    end

    test "drop! returns :ok on 404" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.Graph.drop!(client, "g")
    end
  end
end
