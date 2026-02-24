defmodule Toast.Client.AdminTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  defp client_with_plug(plug) do
    Client.new("http://localhost:8529", plug: plug)
  end

  describe "version/1" do
    test "sends GET to /_api/version" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/version"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, Jason.encode!(%{"server" => "arango", "version" => "3.12.0"}))
      end

      client = client_with_plug(plug)
      assert {:ok, %{"version" => "3.12.0"}} = Client.Admin.version(client)
    end
  end

  describe "status/1" do
    test "sends GET to /_admin/status" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_admin/status"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, Jason.encode!(%{"operationMode" => "server"}))
      end

      client = client_with_plug(plug)
      assert {:ok, %{"operationMode" => "server"}} = Client.Admin.status(client)
    end
  end
end
