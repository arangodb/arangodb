defmodule Toast.Client.IndexTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  defp client_with_plug(plug) do
    Client.new("http://localhost:8529", plug: plug)
  end

  describe "create/3" do
    test "sends POST to /_api/index with collection param" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/index"
        params = URI.decode_query(conn.query_string)
        assert params["collection"] == "users"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(201, Jason.encode!(%{"id" => "users/123"}))
      end

      client = client_with_plug(plug)

      assert {:ok, %{"id" => "users/123"}} =
               Client.Index.create(client, "users", %{"type" => "hash", "fields" => ["name"]})
    end
  end

  describe "list/2" do
    test "sends GET to /_api/index with collection param" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/index"
        params = URI.decode_query(conn.query_string)
        assert params["collection"] == "users"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, Jason.encode!(%{"indexes" => [%{"id" => "users/0"}]}))
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"id" => "users/0"}]} = Client.Index.list(client, "users")
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/index/{handle}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/index/users/123"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, "{}")
      end

      client = client_with_plug(plug)
      assert :ok = Client.Index.drop(client, "users/123")
    end
  end
end
