defmodule Toast.Client.DatabaseTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "list/1" do
    test "sends GET to /_api/database and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/database"

        send_encoded_response(conn, 200, %{"result" => ["_system", "mydb"]})
      end

      client = client_with_plug(plug)
      assert {:ok, ["_system", "mydb"]} = Client.Database.list(client)
    end

    test "returns error on failure" do
      client = client_with_plug(json_plug(403, %{"error" => true}))
      assert {:error, _} = Client.Database.list(client)
    end
  end

  describe "list_accessible/1" do
    test "sends GET to /_api/database/user and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/database/user"

        send_encoded_response(conn, 200, %{"result" => ["_system"]})
      end

      client = client_with_plug(plug)
      assert {:ok, ["_system"]} = Client.Database.list_accessible(client)
    end
  end

  describe "current/1" do
    test "sends GET to /_api/database/current and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/database/current"

        send_encoded_response(conn, 200, %{
          "result" => %{"name" => "_system", "id" => "1", "isSystem" => true}
        })
      end

      client = client_with_plug(plug)
      assert {:ok, %{"name" => "_system"}} = Client.Database.current(client)
    end
  end

  describe "create/2" do
    test "sends POST to /_api/database with name" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/database"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "newdb"

        send_encoded_response(conn, 201, %{"result" => true})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Database.create(client, "newdb")
    end
  end

  describe "create/3" do
    test "passes users option through to request body" do
      users = [%{"username" => "admin", "passwd" => "secret", "active" => true}]

      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "newdb"
        assert decoded["users"] == users

        send_encoded_response(conn, 201, %{"result" => true})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Database.create(client, "newdb", users: users)
    end

    test "passes options option through to request body" do
      options = %{"replicationFactor" => 3, "sharding" => "flexible", "writeConcern" => 2}

      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "newdb"
        assert decoded["options"] == options

        send_encoded_response(conn, 201, %{"result" => true})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Database.create(client, "newdb", options: options)
    end

    test "passes both users and options" do
      users = [%{"username" => "root", "passwd" => "", "active" => true}]
      options = %{"replicationFactor" => 2}

      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "newdb"
        assert decoded["users"] == users
        assert decoded["options"] == options

        send_encoded_response(conn, 201, %{"result" => true})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Database.create(client, "newdb", users: users, options: options)
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/database/{name}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/database/mydb"

        send_encoded_response(conn, 200, %{"result" => true})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Database.drop(client, "mydb")
    end

    test "404 is treated as success (idempotent)" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.Database.drop(client, "nonexistent")
    end
  end

  describe "bang variants" do
    test "list_accessible! returns unwrapped value on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => ["_system"]})
      end

      client = client_with_plug(plug)
      assert ["_system"] = Client.Database.list_accessible!(client)
    end

    test "current! returns unwrapped value on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => %{"name" => "_system"}})
      end

      client = client_with_plug(plug)
      assert %{"name" => "_system"} = Client.Database.current!(client)
    end

    test "list! returns unwrapped value on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => ["_system"]})
      end

      client = client_with_plug(plug)
      assert ["_system"] = Client.Database.list!(client)
    end

    test "list! raises on error" do
      client = client_with_plug(json_plug(403, %{"error" => true}))

      assert_raise RuntimeError, fn ->
        Client.Database.list!(client)
      end
    end

    test "create! returns :ok on success" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 201, %{"result" => true})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Database.create!(client, "newdb")
    end

    test "create! raises on error" do
      client = client_with_plug(json_plug(409, %{"error" => true}))

      assert_raise RuntimeError, fn ->
        Client.Database.create!(client, "existing")
      end
    end

    test "drop! returns :ok on 404 (idempotent)" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.Database.drop!(client, "nonexistent")
    end
  end
end
