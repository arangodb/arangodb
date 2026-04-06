defmodule Toast.Client.AdminTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "version/1" do
    test "sends GET to /_api/version" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/version"

        send_encoded_response(conn, 200, %{"server" => "arango", "version" => "3.12.0"})
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

        send_encoded_response(conn, 200, %{"operationMode" => "server"})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"operationMode" => "server"}} = Client.Admin.status(client)
    end
  end
end
