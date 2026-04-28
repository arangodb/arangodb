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

defmodule Toast.Client.CollectionTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "create/2" do
    test "sends POST to /_api/collection with name and type 2" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/collection"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "test_coll"
        assert decoded["type"] == 2

        send_encoded_response(conn, 200, %{"id" => "123", "name" => "test_coll"})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"name" => "test_coll"}} = Client.Collection.create(client, "test_coll")
    end

    test "passes wait_for_sync as body field" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["waitForSync"] == true

        send_encoded_response(conn, 200, %{"name" => "col"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Collection.create(client, "col", wait_for_sync: true)
    end

    test "passes is_system as body field" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["isSystem"] == true

        send_encoded_response(conn, 200, %{"name" => "_col"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Collection.create(client, "_col", is_system: true)
    end

    test "passes cluster options" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["numberOfShards"] == 3
        assert decoded["replicationFactor"] == 2
        assert decoded["writeConcern"] == 1
        assert decoded["shardKeys"] == ["_key", "region"]
        assert decoded["distributeShardsLike"] == "prototype"

        send_encoded_response(conn, 200, %{"name" => "sharded"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Collection.create(client, "sharded",
                 number_of_shards: 3,
                 replication_factor: 2,
                 write_concern: 1,
                 shard_keys: ["_key", "region"],
                 distribute_shards_like: "prototype"
               )
    end

    test "passes schema as body field" do
      schema = %{"rule" => %{"type" => "object"}, "level" => "strict"}

      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["schema"] == schema

        send_encoded_response(conn, 200, %{"name" => "validated"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Collection.create(client, "validated", schema: schema)
    end

    test "passes key_options as body field" do
      key_options = %{"type" => "uuid", "allowUserKeys" => false}

      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["keyOptions"] == key_options

        send_encoded_response(conn, 200, %{"name" => "col"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Collection.create(client, "col", key_options: key_options)
    end

    test "passes computed_values as body field" do
      computed = [%{"name" => "ts", "expression" => "RETURN DATE_NOW()", "overwrite" => true}]

      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["computedValues"] == computed

        send_encoded_response(conn, 200, %{"name" => "col"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Collection.create(client, "col", computed_values: computed)
    end

    test "uses client's api_version in URL" do
      plug = fn conn ->
        assert conn.request_path == "/_arango/v1/_api/collection"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug, api_version: 1)
      Client.Collection.create(client, "test")
    end
  end

  describe "create_edge/2" do
    test "sends POST with type 3" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "edges"
        assert decoded["type"] == 3

        send_encoded_response(conn, 200, %{"id" => "456", "name" => "edges"})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"name" => "edges"}} = Client.Collection.create_edge(client, "edges")
    end

    test "accepts same opts as create" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["type"] == 3
        assert decoded["numberOfShards"] == 5

        send_encoded_response(conn, 200, %{"name" => "e"})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Collection.create_edge(client, "e", number_of_shards: 5)
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/collection/{name}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/collection/test_coll"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.drop(client, "test_coll")
    end

    test "404 is treated as success (idempotent)" do
      plug = fn conn ->
        send_encoded_response(conn, 404, %{"error" => true})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.drop(client, "nonexistent")
    end

    test "passes is_system as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["isSystem"] == "true"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.drop(client, "_system_coll", is_system: true)
    end
  end

  describe "list/1" do
    test "sends GET to /_api/collection" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/collection"

        send_encoded_response(conn, 200, %{"result" => [%{"name" => "test"}]})
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"name" => "test"}]} = Client.Collection.list(client)
    end

    test "passes exclude_system as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["excludeSystem"] == "true"

        send_encoded_response(conn, 200, %{"result" => []})
      end

      client = client_with_plug(plug)
      assert {:ok, []} = Client.Collection.list(client, exclude_system: true)
    end
  end

  describe "info/2" do
    test "sends GET to /_api/collection/{name}" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/collection/test_coll"

        send_encoded_response(conn, 200, %{"name" => "test_coll", "type" => 2})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"name" => "test_coll"}} = Client.Collection.info(client, "test_coll")
    end
  end

  describe "properties/2" do
    test "sends GET to /_api/collection/{name}/properties" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/collection/test_coll/properties"

        send_encoded_response(conn, 200, %{"name" => "test_coll", "waitForSync" => false})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"waitForSync" => false}} = Client.Collection.properties(client, "test_coll")
    end
  end

  describe "count/2" do
    test "sends GET to /_api/collection/{name}/count and unwraps count" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/collection/test_coll/count"

        send_encoded_response(conn, 200, %{"count" => 42})
      end

      client = client_with_plug(plug)
      assert {:ok, 42} = Client.Collection.count(client, "test_coll")
    end
  end

  describe "truncate/2" do
    test "sends PUT to /_api/collection/{name}/truncate" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/collection/test_coll/truncate"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.truncate(client, "test_coll")
    end

    test "passes wait_for_sync as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["waitForSync"] == "true"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.truncate(client, "col", wait_for_sync: true)
    end

    test "passes compact as query param" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["compact"] == "false"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.truncate(client, "col", compact: false)
    end
  end

  describe "update_properties/3" do
    test "sends PUT to /_api/collection/{name}/properties" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/collection/test_coll/properties"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["schema"] == %{}

        send_encoded_response(conn, 200, %{"name" => "test_coll"})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"name" => "test_coll"}} =
               Client.Collection.update_properties(client, "test_coll", %{"schema" => %{}})
    end
  end

  describe "bang variants" do
    test "create! returns unwrapped value on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"name" => "test"})
      end

      client = client_with_plug(plug)
      assert %{"name" => "test"} = Client.Collection.create!(client, "test")
    end

    test "create! raises on error" do
      plug = fn conn ->
        send_encoded_response(conn, 500, %{"error" => true})
      end

      client = client_with_plug(plug)

      assert_raise RuntimeError, ~r/Collection\.create failed/, fn ->
        Client.Collection.create!(client, "test")
      end
    end

    test "create_edge! returns unwrapped value" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 200, %{"name" => "edges"})
      end

      client = client_with_plug(plug)
      assert %{"name" => "edges"} = Client.Collection.create_edge!(client, "edges")
    end

    test "drop! returns :ok on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.drop!(client, "test")
    end

    test "list! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => [%{"name" => "a"}]})
      end

      client = client_with_plug(plug)
      assert [%{"name" => "a"}] = Client.Collection.list!(client)
    end

    test "truncate! returns :ok on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Collection.truncate!(client, "test")
    end
  end
end
