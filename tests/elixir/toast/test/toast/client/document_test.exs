defmodule Toast.Client.DocumentTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "insert/3" do
    test "sends POST to /_api/document/{collection}" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/document/users"
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        decoded = Jason.decode!(body)
        assert decoded["name"] == "Alice"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(202, Jason.encode!(%{"_key" => "123"}))
      end

      client = client_with_plug(plug)

      assert {:ok, %{"_key" => "123"}} =
               Client.Document.insert(client, "users", %{"name" => "Alice"})
    end
  end

  describe "get/3" do
    test "sends GET to /_api/document/{collection}/{key}" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/document/users/123"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, Jason.encode!(%{"_key" => "123", "name" => "Alice"}))
      end

      client = client_with_plug(plug)
      assert {:ok, %{"_key" => "123"}} = Client.Document.get(client, "users", "123")
    end
  end

  describe "remove/3" do
    test "sends DELETE to /_api/document/{collection}/{key}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/document/users/123"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, "{}")
      end

      client = client_with_plug(plug)
      assert :ok = Client.Document.remove(client, "users", "123")
    end
  end
end
