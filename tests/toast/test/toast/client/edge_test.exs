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

defmodule Toast.Client.EdgeTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "insert/4" do
    test "sends POST to /_api/gharial/{graph}/edge/{coll} and unwraps edge" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/gharial/g/edge/ecol"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["_from"] == "v1/a"
        assert decoded["_to"] == "v2/b"

        send_encoded_response(conn, 202, %{"edge" => %{"_key" => "e1"}})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"_key" => "e1"}} =
               Client.Edge.insert(client, "g", "ecol", %{"_from" => "v1/a", "_to" => "v2/b"})
    end

    test "passes opts as query params" do
      plug = fn conn ->
        params = URI.decode_query(conn.query_string)
        assert params["waitForSync"] == "true"
        {_decoded, conn} = decode_request_body(conn)

        send_encoded_response(conn, 202, %{"edge" => %{"_key" => "e1"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Edge.insert(client, "g", "e", %{"_from" => "a/1", "_to" => "b/1"},
                 wait_for_sync: true
               )
    end
  end

  describe "get/4" do
    test "sends GET and unwraps edge" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/gharial/g/edge/ecol/e1"

        send_encoded_response(conn, 200, %{"edge" => %{"_key" => "e1", "_from" => "v1/a"}})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"_key" => "e1"}} = Client.Edge.get(client, "g", "ecol", "e1")
    end
  end

  describe "update/5" do
    test "sends PATCH and unwraps edge" do
      plug = fn conn ->
        assert conn.method == "PATCH"
        assert conn.request_path == "/_api/gharial/g/edge/ecol/e1"

        send_encoded_response(conn, 200, %{"edge" => %{"_key" => "e1"}})
      end

      client = client_with_plug(plug)
      assert {:ok, _} = Client.Edge.update(client, "g", "ecol", "e1", %{weight: 5})
    end
  end

  describe "replace/5" do
    test "sends PUT and unwraps edge" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/gharial/g/edge/ecol/e1"

        send_encoded_response(conn, 200, %{"edge" => %{"_key" => "e1"}})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Edge.replace(client, "g", "ecol", "e1", %{
                 "_from" => "v1/a",
                 "_to" => "v2/c",
                 "weight" => 10
               })
    end
  end

  describe "remove/4" do
    test "sends DELETE and returns :ok" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/gharial/g/edge/ecol/e1"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Edge.remove(client, "g", "ecol", "e1")
    end
  end

  describe "bang variants" do
    test "insert! returns unwrapped value" do
      plug = fn conn ->
        {_decoded, conn} = decode_request_body(conn)
        send_encoded_response(conn, 202, %{"edge" => %{"_key" => "e1"}})
      end

      client = client_with_plug(plug)

      assert %{"_key" => "e1"} =
               Client.Edge.insert!(client, "g", "e", %{"_from" => "a/1", "_to" => "b/1"})
    end

    test "get! raises on error" do
      client = client_with_plug(json_plug(404, %{"error" => true}))

      assert_raise RuntimeError, ~r/Edge\.get failed/, fn ->
        Client.Edge.get!(client, "g", "e", "1")
      end
    end

    test "remove! returns :ok" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Edge.remove!(client, "g", "e", "1")
    end
  end
end
