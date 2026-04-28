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

defmodule Toast.Client.UserTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "list/1" do
    test "sends GET to /_api/user/ and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/user/"

        send_encoded_response(conn, 200, %{"result" => [%{"user" => "root"}]})
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"user" => "root"}]} = Client.User.list(client)
    end
  end

  describe "create/2" do
    test "sends POST to /_api/user with username" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/user"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["user"] == "testuser"

        send_encoded_response(conn, 201, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.create(client, "testuser")
    end
  end

  describe "create/3" do
    test "includes opts in body" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["user"] == "testuser"
        assert decoded["passwd"] == "secret"
        assert decoded["active"] == false
        assert decoded["extra"] == %{"role" => "admin"}

        send_encoded_response(conn, 201, %{})
      end

      client = client_with_plug(plug)

      assert :ok =
               Client.User.create(client, "testuser",
                 passwd: "secret",
                 active: false,
                 extra: %{"role" => "admin"}
               )
    end
  end

  describe "get/2" do
    test "sends GET to /_api/user/{name}" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/user/testuser"

        send_encoded_response(conn, 200, %{"user" => "testuser", "active" => true})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"user" => "testuser"}} = Client.User.get(client, "testuser")
    end
  end

  describe "update/3" do
    test "sends PATCH to /_api/user/{name} with opts" do
      plug = fn conn ->
        assert conn.method == "PATCH"
        assert conn.request_path == "/_api/user/testuser"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["active"] == false
        assert decoded["passwd"] == "newpass"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.update(client, "testuser", active: false, passwd: "newpass")
    end
  end

  describe "replace/3" do
    test "sends PUT to /_api/user/{name} with body" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/user/testuser"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["passwd"] == "newpass"
        assert decoded["active"] == true

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.replace(client, "testuser", passwd: "newpass", active: true)
    end

    test "omits missing opts from body" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["passwd"] == "x"
        refute Map.has_key?(decoded, "active")
        refute Map.has_key?(decoded, "extra")

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.replace(client, "u", passwd: "x")
    end
  end

  describe "list_databases/2" do
    test "sends GET to /_api/user/{name}/database and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/user/testuser/database"

        send_encoded_response(conn, 200, %{"result" => %{"mydb" => "rw", "_system" => "rw"}})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"mydb" => "rw"}} = Client.User.list_databases(client, "testuser")
    end

    test "passes full as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["full"] == "true"

        send_encoded_response(conn, 200, %{"result" => %{}})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.User.list_databases(client, "testuser", full: true)
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/user/{name}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/user/testuser"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.drop(client, "testuser")
    end

    test "404 is treated as success (idempotent)" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.User.drop(client, "nonexistent")
    end
  end

  describe "database_access/3" do
    test "sends GET and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/user/testuser/database/mydb"

        send_encoded_response(conn, 200, %{"result" => "rw"})
      end

      client = client_with_plug(plug)
      assert {:ok, "rw"} = Client.User.database_access(client, "testuser", "mydb")
    end
  end

  describe "grant_database_access/4" do
    test "sends PUT with grant body" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/user/testuser/database/mydb"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["grant"] == "rw"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.grant_database_access(client, "testuser", "mydb", :rw)
    end

    test "converts atom levels to strings" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["grant"] == "ro"
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.grant_database_access(client, "u", "db", :ro)
    end
  end

  describe "revoke_database_access/3" do
    test "sends DELETE" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/user/testuser/database/mydb"
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.revoke_database_access(client, "testuser", "mydb")
    end
  end

  describe "collection_access/4" do
    test "sends GET and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/user/testuser/database/mydb/col"

        send_encoded_response(conn, 200, %{"result" => "ro"})
      end

      client = client_with_plug(plug)
      assert {:ok, "ro"} = Client.User.collection_access(client, "testuser", "mydb", "col")
    end
  end

  describe "grant_collection_access/5" do
    test "sends PUT with grant body" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/user/testuser/database/mydb/col"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["grant"] == "none"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.grant_collection_access(client, "testuser", "mydb", "col", :none)
    end
  end

  describe "revoke_collection_access/4" do
    test "sends DELETE" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/user/testuser/database/mydb/col"
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.revoke_collection_access(client, "testuser", "mydb", "col")
    end
  end

  describe "bang variants" do
    test "list! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => [%{"user" => "root"}]})
      end

      client = client_with_plug(plug)
      assert [%{"user" => "root"}] = Client.User.list!(client)
    end

    test "create! raises on error" do
      client = client_with_plug(json_plug(500, %{"error" => true}))

      assert_raise RuntimeError, ~r/User\.create failed/, fn ->
        Client.User.create!(client, "u")
      end
    end

    test "get! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"user" => "testuser", "active" => true})
      end

      client = client_with_plug(plug)
      assert %{"user" => "testuser"} = Client.User.get!(client, "testuser")
    end

    test "update! returns :ok" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.update!(client, "u", active: true)
    end

    test "replace! returns :ok" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.replace!(client, "u", passwd: "x")
    end

    test "drop! returns :ok on 404" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.User.drop!(client, "u")
    end

    test "list_databases! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => %{"mydb" => "rw"}})
      end

      client = client_with_plug(plug)
      assert %{"mydb" => "rw"} = Client.User.list_databases!(client, "u")
    end

    test "database_access! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => "rw"})
      end

      client = client_with_plug(plug)
      assert "rw" = Client.User.database_access!(client, "u", "db")
    end

    test "grant_database_access! returns :ok" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.grant_database_access!(client, "u", "db", :rw)
    end

    test "revoke_database_access! returns :ok" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.revoke_database_access!(client, "u", "db")
    end

    test "collection_access! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => "ro"})
      end

      client = client_with_plug(plug)
      assert "ro" = Client.User.collection_access!(client, "u", "db", "col")
    end

    test "grant_collection_access! returns :ok" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.grant_collection_access!(client, "u", "db", "col", :ro)
    end

    test "revoke_collection_access! returns :ok" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.User.revoke_collection_access!(client, "u", "db", "col")
    end
  end
end
