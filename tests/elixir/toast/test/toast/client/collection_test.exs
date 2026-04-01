defmodule Toast.Client.CollectionTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "create/3" do
    test "sends POST to /_api/collection with name and type" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/collection"
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        decoded = Jason.decode!(body)
        assert decoded["name"] == "test_coll"
        assert decoded["type"] == 2

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, Jason.encode!(%{"id" => "123", "name" => "test_coll"}))
      end

      client = client_with_plug(plug)
      assert {:ok, %{"name" => "test_coll"}} = Client.Collection.create(client, "test_coll")
    end

    test "edge: true sets type 3" do
      plug = fn conn ->
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        decoded = Jason.decode!(body)
        assert decoded["type"] == 3

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, Jason.encode!(%{"id" => "456", "name" => "edges"}))
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Collection.create(client, "edges", edge: true)
    end

    test "uses client's api_version in URL" do
      plug = fn conn ->
        assert conn.request_path == "/_arango/v1/_api/collection"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, Jason.encode!(%{}))
      end

      client = client_with_plug(plug, api_version: 1)
      Client.Collection.create(client, "test")
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/collection/{name}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/collection/test_coll"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, "{}")
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.drop(client, "test_coll")
    end

    test "404 is treated as success (idempotent)" do
      plug = fn conn ->
        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(404, ~s({"error":true}))
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.drop(client, "nonexistent")
    end
  end

  describe "list/1" do
    test "sends GET to /_api/collection" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/collection"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(
          200,
          Jason.encode!(%{"result" => [%{"name" => "test"}]})
        )
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"name" => "test"}]} = Client.Collection.list(client)
    end
  end
end
