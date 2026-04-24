defmodule Toast.Client.IndexTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "ensure/4" do
    test "sends POST to /_api/index with type, fields, and collection param" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/index"
        params = URI.decode_query(conn.query_string)
        assert params["collection"] == "users"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["type"] == "persistent"
        assert decoded["fields"] == ["email"]

        send_encoded_response(conn, 201, %{"id" => "users/123"})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"id" => "users/123"}} =
               Client.Index.ensure(client, "users", :persistent, ["email"])
    end

    test "succeeds when index already exists (200)" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"id" => "users/123"})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"id" => "users/123"}} =
               Client.Index.ensure(client, "users", :persistent, ["email"])
    end

    test "passes unique and sparse opts" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["unique"] == true
        assert decoded["sparse"] == true

        send_encoded_response(conn, 201, %{"id" => "users/123"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Index.ensure(client, "users", :persistent, ["email"],
                 unique: true,
                 sparse: true
               )
    end

    test "passes name opt" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "idx_email"

        send_encoded_response(conn, 201, %{"id" => "users/123"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Index.ensure(client, "users", :persistent, ["email"], name: "idx_email")
    end

    test "passes TTL-specific expire_after opt" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["type"] == "ttl"
        assert decoded["expireAfter"] == 3600

        send_encoded_response(conn, 201, %{"id" => "users/456"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Index.ensure(client, "users", :ttl, ["createdAt"], expire_after: 3600)
    end

    test "passes cache_enabled and in_background opts" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["cacheEnabled"] == true
        assert decoded["inBackground"] == true

        send_encoded_response(conn, 201, %{"id" => "users/789"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Index.ensure(client, "users", :persistent, ["name"],
                 cache_enabled: true,
                 in_background: true
               )
    end

    test "passes deduplicate and estimates opts" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["deduplicate"] == false
        assert decoded["estimates"] == false

        send_encoded_response(conn, 201, %{"id" => "users/101"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Index.ensure(client, "users", :persistent, ["tags[*]"],
                 deduplicate: false,
                 estimates: false
               )
    end

    test "passes geo_json and legacy_polygons opts" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["type"] == "geo"
        assert decoded["geoJson"] == true
        assert decoded["legacyPolygons"] == false

        send_encoded_response(conn, 201, %{"id" => "places/1"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Index.ensure(client, "places", :geo, ["location"],
                 geo_json: true,
                 legacy_polygons: false
               )
    end

    test "passes stored_values opt" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["storedValues"] == ["name", "email"]

        send_encoded_response(conn, 201, %{"id" => "users/102"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Index.ensure(client, "users", :persistent, ["age"],
                 stored_values: ["name", "email"]
               )
    end
  end

  describe "list/2" do
    test "sends GET to /_api/index with collection param" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/index"
        params = URI.decode_query(conn.query_string)
        assert params["collection"] == "users"

        send_encoded_response(conn, 200, %{"indexes" => [%{"id" => "users/0"}]})
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"id" => "users/0"}]} = Client.Index.list(client, "users")
    end

    test "passes with_stats as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["withStats"] == "true"

        send_encoded_response(conn, 200, %{"indexes" => []})
      end

      client = client_with_plug(plug)
      assert {:ok, []} = Client.Index.list(client, "users", with_stats: true)
    end

    test "passes with_hidden as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["withHidden"] == "true"

        send_encoded_response(conn, 200, %{"indexes" => []})
      end

      client = client_with_plug(plug)
      assert {:ok, []} = Client.Index.list(client, "users", with_hidden: true)
    end
  end

  describe "get/2" do
    test "sends GET to /_api/index/{handle}" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/index/users/123"

        send_encoded_response(conn, 200, %{"id" => "users/123", "type" => "persistent"})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"id" => "users/123", "type" => "persistent"}} =
               Client.Index.get(client, "users/123")
    end

    test "returns error on 404" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert {:error, _} = Client.Index.get(client, "users/999")
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/index/{handle}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/index/users/123"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Index.drop(client, "users/123")
    end

    test "404 is treated as success (idempotent)" do
      plug = fn conn ->
        send_encoded_response(conn, 404, %{"error" => true})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Index.drop(client, "users/999")
    end
  end

  describe "bang variants" do
    test "ensure! returns unwrapped value on success" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 201, %{"id" => "users/123"})
      end

      client = client_with_plug(plug)

      assert %{"id" => "users/123"} =
               Client.Index.ensure!(client, "users", :persistent, ["name"])
    end

    test "ensure! raises on error" do
      client = client_with_plug(json_plug(500, %{"error" => true}))

      assert_raise RuntimeError, ~r/Index\.ensure failed/, fn ->
        Client.Index.ensure!(client, "users", :persistent, ["name"])
      end
    end

    test "list! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"indexes" => [%{"id" => "users/0"}]})
      end

      client = client_with_plug(plug)
      assert [%{"id" => "users/0"}] = Client.Index.list!(client, "users")
    end

    test "get! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"id" => "users/123", "type" => "persistent"})
      end

      client = client_with_plug(plug)
      assert %{"id" => "users/123"} = Client.Index.get!(client, "users/123")
    end

    test "drop! returns :ok on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Index.drop!(client, "users/123")
    end
  end
end
