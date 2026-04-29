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

  describe "bang variants" do
    test "version! returns unwrapped value on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"version" => "3.12.0"})
      end

      client = client_with_plug(plug)
      assert %{"version" => "3.12.0"} = Client.Admin.version!(client)
    end

    test "version! raises on error" do
      plug = fn conn ->
        send_encoded_response(conn, 500, %{"error" => true})
      end

      client = client_with_plug(plug)

      assert_raise RuntimeError, ~r/Admin\.version failed/, fn ->
        Client.Admin.version!(client)
      end
    end

    test "status! returns unwrapped value on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"operationMode" => "server"})
      end

      client = client_with_plug(plug)
      assert %{"operationMode" => "server"} = Client.Admin.status!(client)
    end
  end
end
