defmodule Toast.Client.DocumentTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "insert/3" do
    test "sends POST to /_api/document/{collection}" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/document/users"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "Alice"

        send_encoded_response(conn, 202, %{"_key" => "123"})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"_key" => "123"}} =
               Client.Document.insert(client, "users", %{"name" => "Alice"})
    end

    test "passes opts as query params" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["waitForSync"] == "true"
        assert params["returnNew"] == "true"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 202, %{"_key" => "1"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Document.insert(client, "col", %{}, wait_for_sync: true, return_new: true)
    end
  end

  describe "get/3" do
    test "sends GET to /_api/document/{collection}/{key}" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/document/users/123"

        send_encoded_response(conn, 200, %{"_key" => "123", "name" => "Alice"})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"_key" => "123"}} = Client.Document.get(client, "users", "123")
    end

    test "passes if_match as If-Match header" do
      plug = fn conn ->
        [etag] = Plug.Conn.get_req_header(conn, "if-match")
        send(self(), {:if_match, etag})

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      Client.Document.get(client, "col", "1", headers: [if_match: "\"_rev123\""])
      assert_received {:if_match, "\"_rev123\""}
    end

    test "passes if_none_match as If-None-Match header" do
      plug = fn conn ->
        [etag] = Plug.Conn.get_req_header(conn, "if-none-match")
        send(self(), {:if_none_match, etag})

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      Client.Document.get(client, "col", "1", headers: [if_none_match: "\"_rev456\""])
      assert_received {:if_none_match, "\"_rev456\""}
    end
  end

  describe "update/4" do
    test "sends PATCH to /_api/document/{collection}/{key}" do
      plug = fn conn ->
        assert conn.method == "PATCH"
        assert conn.request_path == "/_api/document/users/123"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "Bob"

        send_encoded_response(conn, 200, %{"_key" => "123"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Document.update(client, "users", "123", %{"name" => "Bob"})
    end

    test "passes opts as query params" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["returnOld"] == "true"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Document.update(client, "c", "1", %{}, return_old: true)
    end

    test "passes keep_null as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["keepNull"] == "false"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Document.update(client, "c", "1", %{}, keep_null: false)
    end

    test "passes merge_objects as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["mergeObjects"] == "false"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Document.update(client, "c", "1", %{}, merge_objects: false)
    end

    test "passes ignore_revs as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["ignoreRevs"] == "false"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Document.update(client, "c", "1", %{}, ignore_revs: false)
    end

    test "passes if_match as If-Match header" do
      plug = fn conn ->
        [etag] = Plug.Conn.get_req_header(conn, "if-match")
        send(self(), {:if_match, etag})
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      Client.Document.update(client, "c", "1", %{}, headers: [if_match: "\"_rev123\""])
      assert_received {:if_match, "\"_rev123\""}
    end
  end

  describe "replace/4" do
    test "sends PUT to /_api/document/{collection}/{key}" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/document/users/123"

        send_encoded_response(conn, 200, %{"_key" => "123"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Document.replace(client, "users", "123", %{"name" => "Charlie"})
    end

    test "passes ignore_revs as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["ignoreRevs"] == "false"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Document.replace(client, "c", "1", %{}, ignore_revs: false)
    end

    test "passes if_match as If-Match header" do
      plug = fn conn ->
        [etag] = Plug.Conn.get_req_header(conn, "if-match")
        send(self(), {:if_match, etag})
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      Client.Document.replace(client, "c", "1", %{}, headers: [if_match: "\"_rev123\""])
      assert_received {:if_match, "\"_rev123\""}
    end
  end

  describe "remove/3" do
    test "sends DELETE to /_api/document/{collection}/{key}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/document/users/123"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Document.remove(client, "users", "123")
    end

    test "passes opts as query params" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["waitForSync"] == "true"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Document.remove(client, "c", "1", wait_for_sync: true)
    end

    test "passes ignore_revs as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["ignoreRevs"] == "false"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Document.remove(client, "c", "1", ignore_revs: false)
    end

    test "passes if_match as If-Match header" do
      plug = fn conn ->
        [etag] = Plug.Conn.get_req_header(conn, "if-match")
        send(self(), {:if_match, etag})

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      Client.Document.remove(client, "c", "1", headers: [if_match: "\"_rev123\""])
      assert_received {:if_match, "\"_rev123\""}
    end
  end

  describe "insert_many/3" do
    test "sends POST with list body" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/document/users"
        {decoded, conn} = decode_request_body(conn)
        assert length(decoded) == 2

        send_encoded_response(conn, 202, [%{"_key" => "1"}, %{"_key" => "2"}])
      end

      client = client_with_plug(plug)
      docs = [%{"name" => "A"}, %{"name" => "B"}]

      assert {:ok, [%{"_key" => "1"}, %{"_key" => "2"}]} =
               Client.Document.insert_many(client, "users", docs)
    end
  end

  describe "update_many/3" do
    test "sends PATCH to /_api/document/{collection} with list body" do
      plug = fn conn ->
        assert conn.method == "PATCH"
        assert conn.request_path == "/_api/document/users"
        {decoded, conn} = decode_request_body(conn)
        assert [%{"_key" => "1", "name" => "Updated"}] = decoded

        send_encoded_response(conn, 200, [%{"_key" => "1"}])
      end

      client = client_with_plug(plug)

      assert {:ok, [%{"_key" => "1"}]} =
               Client.Document.update_many(client, "users", [
                 %{"_key" => "1", "name" => "Updated"}
               ])
    end
  end

  describe "replace_many/3" do
    test "sends PUT to /_api/document/{collection} with list body" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/document/users"

        send_encoded_response(conn, 200, [%{"_key" => "1"}])
      end

      client = client_with_plug(plug)

      assert {:ok, [%{"_key" => "1"}]} =
               Client.Document.replace_many(client, "users", [%{"_key" => "1", "name" => "New"}])
    end
  end

  describe "remove_many/3" do
    test "sends DELETE to /_api/document/{collection} with body" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/document/users"
        {decoded, conn} = decode_request_body(conn)
        assert decoded == ["key1", "key2"]

        send_encoded_response(conn, 200, [%{"_key" => "key1"}, %{"_key" => "key2"}])
      end

      client = client_with_plug(plug)

      assert {:ok, [_, _]} = Client.Document.remove_many(client, "users", ["key1", "key2"])
    end

    test "passes opts as query params" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["waitForSync"] == "true"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 200, [%{"_key" => "1"}])
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Document.remove_many(client, "c", ["1"], wait_for_sync: true)
    end
  end

  describe "bang variants" do
    test "insert! returns unwrapped value" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 202, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      assert %{"_key" => "1"} = Client.Document.insert!(client, "col", %{})
    end

    test "insert! raises on error" do
      client = client_with_plug(json_plug(500, %{"error" => true}))

      assert_raise RuntimeError, ~r/Document\.insert failed/, fn ->
        Client.Document.insert!(client, "col", %{})
      end
    end

    test "get! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"_key" => "1"})
      end

      client = client_with_plug(plug)
      assert %{"_key" => "1"} = Client.Document.get!(client, "col", "1")
    end

    test "remove! returns :ok" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Document.remove!(client, "col", "1")
    end

    test "insert_many! returns unwrapped value" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 202, [%{"_key" => "1"}])
      end

      client = client_with_plug(plug)
      assert [%{"_key" => "1"}] = Client.Document.insert_many!(client, "col", [%{}])
    end

    test "remove_many! returns unwrapped value" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 200, [%{"_key" => "1"}])
      end

      client = client_with_plug(plug)
      assert [%{"_key" => "1"}] = Client.Document.remove_many!(client, "col", ["1"])
    end
  end
end
