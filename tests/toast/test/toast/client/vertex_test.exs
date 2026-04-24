defmodule Toast.Client.VertexTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "insert/4" do
    test "sends POST to /_api/gharial/{graph}/vertex/{coll} and unwraps vertex" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/gharial/g/vertex/vcol"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "Alice"

        send_encoded_response(conn, 202, %{"vertex" => %{"_key" => "123"}})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"_key" => "123"}} =
               Client.Vertex.insert(client, "g", "vcol", %{name: "Alice"})
    end

    test "passes opts as query params" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["waitForSync"] == "true"
        assert params["returnNew"] == "true"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 202, %{"vertex" => %{"_key" => "1"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Vertex.insert(client, "g", "v", %{}, wait_for_sync: true, return_new: true)
    end
  end

  describe "get/4" do
    test "sends GET and unwraps vertex" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/gharial/g/vertex/vcol/123"

        send_encoded_response(conn, 200, %{"vertex" => %{"_key" => "123", "name" => "Alice"}})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"_key" => "123"}} = Client.Vertex.get(client, "g", "vcol", "123")
    end
  end

  describe "update/5" do
    test "sends PATCH and unwraps vertex" do
      plug = fn conn ->
        assert conn.method == "PATCH"
        assert conn.request_path == "/_api/gharial/g/vertex/vcol/123"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "Bob"

        send_encoded_response(conn, 200, %{"vertex" => %{"_key" => "123"}})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Vertex.update(client, "g", "vcol", "123", %{name: "Bob"})
    end
  end

  describe "replace/5" do
    test "sends PUT and unwraps vertex" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/gharial/g/vertex/vcol/123"

        send_encoded_response(conn, 200, %{"vertex" => %{"_key" => "123"}})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Vertex.replace(client, "g", "vcol", "123", %{name: "Charlie"})
    end
  end

  describe "remove/4" do
    test "sends DELETE and returns :ok" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/gharial/g/vertex/vcol/123"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Vertex.remove(client, "g", "vcol", "123")
    end

    test "passes opts as query params" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["waitForSync"] == "true"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Vertex.remove(client, "g", "v", "1", wait_for_sync: true)
    end
  end

  describe "bang variants" do
    test "insert! returns unwrapped value" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 202, %{"vertex" => %{"_key" => "1"}})
      end

      client = client_with_plug(plug)
      assert %{"_key" => "1"} = Client.Vertex.insert!(client, "g", "v", %{})
    end

    test "get! raises on error" do
      client = client_with_plug(json_plug(404, %{"error" => true}))

      assert_raise RuntimeError, ~r/Vertex\.get failed/, fn ->
        Client.Vertex.get!(client, "g", "v", "1")
      end
    end

    test "remove! returns :ok" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Vertex.remove!(client, "g", "v", "1")
    end
  end
end
