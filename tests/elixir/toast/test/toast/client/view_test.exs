defmodule Toast.Client.ViewTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "list/1" do
    test "sends GET to /_api/view and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/view"

        send_encoded_response(conn, 200, %{"result" => [%{"name" => "myview"}]})
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"name" => "myview"}]} = Client.View.list(client)
    end
  end

  describe "create/3" do
    test "sends POST to /_api/view with name and type" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/view"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "myview"
        assert decoded["type"] == "arangosearch"

        send_encoded_response(conn, 201, %{"name" => "myview", "type" => "arangosearch"})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"name" => "myview"}} = Client.View.create(client, "myview", "arangosearch")
    end
  end

  describe "create/4" do
    test "merges properties into body" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "myview"
        assert decoded["type"] == "arangosearch"
        assert decoded["links"] == %{"col" => %{"analyzers" => ["identity"]}}

        send_encoded_response(conn, 201, %{"name" => "myview"})
      end

      client = client_with_plug(plug)
      props = %{"links" => %{"col" => %{"analyzers" => ["identity"]}}}
      assert {:ok, _} = Client.View.create(client, "myview", "arangosearch", props)
    end
  end

  describe "info/2" do
    test "sends GET to /_api/view/{name}" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/view/myview"

        send_encoded_response(conn, 200, %{"name" => "myview", "type" => "arangosearch"})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"name" => "myview"}} = Client.View.info(client, "myview")
    end
  end

  describe "properties/2" do
    test "sends GET to /_api/view/{name}/properties" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/view/myview/properties"

        send_encoded_response(conn, 200, %{"name" => "myview", "links" => %{}})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"links" => %{}}} = Client.View.properties(client, "myview")
    end
  end

  describe "update_properties/3" do
    test "sends PATCH to /_api/view/{name}/properties" do
      plug = fn conn ->
        assert conn.method == "PATCH"
        assert conn.request_path == "/_api/view/myview/properties"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["links"] == %{"col" => %{}}

        send_encoded_response(conn, 200, %{"name" => "myview"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.View.update_properties(client, "myview", %{"links" => %{"col" => %{}}})
    end
  end

  describe "replace_properties/3" do
    test "sends PUT to /_api/view/{name}/properties" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/view/myview/properties"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["links"] == %{}

        send_encoded_response(conn, 200, %{"name" => "myview"})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"name" => "myview"}} =
               Client.View.replace_properties(client, "myview", %{"links" => %{}})
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/view/{name}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/view/myview"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.View.drop(client, "myview")
    end

    test "404 is treated as success (idempotent)" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.View.drop(client, "nonexistent")
    end
  end

  describe "bang variants" do
    test "list! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => [%{"name" => "v"}]})
      end

      client = client_with_plug(plug)
      assert [%{"name" => "v"}] = Client.View.list!(client)
    end

    test "create! raises on error" do
      client = client_with_plug(json_plug(500, %{"error" => true}))

      assert_raise RuntimeError, ~r/View\.create failed/, fn ->
        Client.View.create!(client, "v", "arangosearch")
      end
    end

    test "info! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"name" => "v", "type" => "arangosearch"})
      end

      client = client_with_plug(plug)
      assert %{"name" => "v"} = Client.View.info!(client, "v")
    end

    test "properties! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"name" => "v", "links" => %{}})
      end

      client = client_with_plug(plug)
      assert %{"links" => %{}} = Client.View.properties!(client, "v")
    end

    test "update_properties! returns unwrapped value" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 200, %{"name" => "v"})
      end

      client = client_with_plug(plug)
      assert %{"name" => "v"} = Client.View.update_properties!(client, "v", %{})
    end

    test "replace_properties! returns unwrapped value" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 200, %{"name" => "v"})
      end

      client = client_with_plug(plug)
      assert %{"name" => "v"} = Client.View.replace_properties!(client, "v", %{})
    end

    test "drop! returns :ok on 404" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.View.drop!(client, "v")
    end
  end
end
