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

defmodule Toast.Client.AnalyzerTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "list/1" do
    test "sends GET to /_api/analyzer and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/analyzer"

        send_encoded_response(conn, 200, %{"result" => [%{"name" => "identity"}]})
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"name" => "identity"}]} = Client.Analyzer.list(client)
    end
  end

  describe "create/3" do
    test "sends POST to /_api/analyzer with name and type" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/analyzer"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "my_analyzer"
        assert decoded["type"] == "text"

        send_encoded_response(conn, 201, %{"name" => "my_analyzer", "type" => "text"})
      end

      client = client_with_plug(plug)

      assert {:ok, %{"name" => "my_analyzer"}} =
               Client.Analyzer.create(client, "my_analyzer", "text")
    end
  end

  describe "create/4" do
    test "includes properties and features in body" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["name"] == "my_analyzer"
        assert decoded["type"] == "text"
        assert decoded["properties"] == %{"locale" => "en"}
        assert decoded["features"] == ["frequency", "position"]

        send_encoded_response(conn, 201, %{"name" => "my_analyzer"})
      end

      client = client_with_plug(plug)

      assert {:ok, _} =
               Client.Analyzer.create(client, "my_analyzer", "text",
                 properties: %{"locale" => "en"},
                 features: ["frequency", "position"]
               )
    end
  end

  describe "get/2" do
    test "sends GET to /_api/analyzer/{name}" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/analyzer/my_analyzer"

        send_encoded_response(conn, 200, %{"name" => "my_analyzer", "type" => "text"})
      end

      client = client_with_plug(plug)
      assert {:ok, %{"name" => "my_analyzer"}} = Client.Analyzer.get(client, "my_analyzer")
    end
  end

  describe "drop/2" do
    test "sends DELETE to /_api/analyzer/{name}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/analyzer/my_analyzer"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Analyzer.drop(client, "my_analyzer")
    end

    test "404 is treated as success (idempotent)" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.Analyzer.drop(client, "nonexistent")
    end
  end

  describe "drop/3" do
    test "passes force query param" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        params = URI.decode_query(conn.query_string)
        assert params["force"] == "true"

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      assert :ok = Client.Analyzer.drop(client, "my_analyzer", force: true)
    end
  end

  describe "bang variants" do
    test "list! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => [%{"name" => "a"}]})
      end

      client = client_with_plug(plug)
      assert [%{"name" => "a"}] = Client.Analyzer.list!(client)
    end

    test "create! raises on error" do
      client = client_with_plug(json_plug(500, %{"error" => true}))

      assert_raise RuntimeError, ~r/Analyzer\.create failed/, fn ->
        Client.Analyzer.create!(client, "a", "text")
      end
    end

    test "get! returns unwrapped value" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"name" => "a"})
      end

      client = client_with_plug(plug)
      assert %{"name" => "a"} = Client.Analyzer.get!(client, "a")
    end

    test "drop! returns :ok on 404" do
      client = client_with_plug(json_plug(404, %{"error" => true}))
      assert :ok = Client.Analyzer.drop!(client, "a")
    end
  end
end
