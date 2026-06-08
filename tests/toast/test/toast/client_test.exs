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

defmodule Toast.ClientTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "new/2" do
    test "returns a Client struct with base_url" do
      client = Client.new("http://localhost:8529")
      assert %Client{base_url: "http://localhost:8529"} = client
    end

    test "pops database from opts" do
      client = Client.new("http://localhost:8529", database: "mydb")
      assert client.database == "mydb"
    end

    test "pops api_version from opts" do
      client = Client.new("http://localhost:8529", api_version: 1)
      assert client.api_version == 1
    end

    test "pops auth from opts" do
      client = Client.new("http://localhost:8529", auth: {:basic, "root", ""})
      assert client.auth == {:basic, "root", ""}
    end

    test "pops content_type from opts" do
      client = Client.new("http://localhost:8529", content_type: :vpack)
      assert client.content_type == :vpack
    end

    test "defaults content_type to :vpack" do
      client = Client.new("http://localhost:8529")
      assert client.content_type == :vpack
    end

    test "passes remaining opts to Req" do
      client = Client.new("http://localhost:8529", receive_timeout: 5_000)
      assert client.req.options.receive_timeout == 5_000
    end

    test "disables retry by default" do
      client = Client.new("http://localhost:8529")
      assert client.req.options.retry == false
    end

    test "defaults protocol to :http1" do
      client = Client.new("http://localhost:8529")
      assert client.protocol == :http1
    end

    test "pops protocol from opts" do
      client = Client.new("http://localhost:8529", protocol: :http2)
      assert client.protocol == :http2
    end

    test "sets http2 connect_options on Req when protocol is :http2" do
      client = Client.new("http://localhost:8529", protocol: :http2)
      assert client.req.options.connect_options == [protocols: [:http2]]
    end

    test "does not set connect_options when protocol is :http1" do
      client = Client.new("http://localhost:8529")
      refute Map.has_key?(client.req.options, :connect_options)
    end
  end

  describe "SSL connect options" do
    test "https base_url disables cert verification" do
      client = Client.new("https://localhost:8529")
      assert client.req.options.connect_options[:transport_opts] == [verify: :verify_none]
    end

    test "http base_url does not set transport_opts" do
      client = Client.new("http://localhost:8529")
      refute Map.has_key?(client.req.options, :connect_options)
    end

    test "https with http2 merges both connect_options" do
      client = Client.new("https://localhost:8529", protocol: :http2)
      connect_opts = client.req.options.connect_options
      assert connect_opts[:protocols] == [:http2]
      assert connect_opts[:transport_opts] == [verify: :verify_none]
    end
  end

  describe "scoping functions" do
    test "with_database returns new client with database set" do
      client = Client.new("http://localhost:8529")
      scoped = Client.with_database(client, "testdb")
      assert scoped.database == "testdb"
      assert client.database == nil
    end

    test "with_auth returns new client with basic auth" do
      client = Client.new("http://localhost:8529")
      scoped = Client.with_auth(client, {:basic, "root", "pass"})
      assert scoped.auth == {:basic, "root", "pass"}
      assert client.auth == nil
    end

    test "with_auth returns new client with JWT auth" do
      client = Client.new("http://localhost:8529")
      scoped = Client.with_auth(client, {:jwt, "my.jwt.token"})
      assert scoped.auth == {:jwt, "my.jwt.token"}
    end

    test "with_api_version with integer" do
      client = Client.new("http://localhost:8529")
      scoped = Client.with_api_version(client, 1)
      assert scoped.api_version == 1
    end

    test "with_api_version with string" do
      client = Client.new("http://localhost:8529")
      scoped = Client.with_api_version(client, "experimental")
      assert scoped.api_version == "experimental"
    end

    test "with_api_version with nil clears version" do
      client = Client.new("http://localhost:8529", api_version: 1)
      scoped = Client.with_api_version(client, nil)
      assert scoped.api_version == nil
    end

    test "with_content_type switches to json" do
      client = Client.new("http://localhost:8529")
      scoped = Client.with_content_type(client, :json)
      assert scoped.content_type == :json
      assert client.content_type == :vpack
    end

    test "with_content_type switches back to vpack" do
      client = Client.new("http://localhost:8529", content_type: :json)
      scoped = Client.with_content_type(client, :vpack)
      assert scoped.content_type == :vpack
    end
  end

  describe "URL construction" do
    test "no version, no database" do
      plug = fn conn ->
        send(self(), {:path, conn.request_path})
        json_plug().(conn)
      end

      client = client_with_plug(plug)
      Client.get(client, "/_api/collection")
      assert_received {:path, "/_api/collection"}
    end

    test "integer api_version, no database" do
      plug = fn conn ->
        send(self(), {:path, conn.request_path})
        json_plug().(conn)
      end

      client = client_with_plug(plug, api_version: 1)
      Client.get(client, "/_api/collection")
      assert_received {:path, "/_arango/v1/_api/collection"}
    end

    test "string api_version, no database" do
      plug = fn conn ->
        send(self(), {:path, conn.request_path})
        json_plug().(conn)
      end

      client = client_with_plug(plug, api_version: "experimental")
      Client.get(client, "/_api/collection")
      assert_received {:path, "/_arango/experimental/_api/collection"}
    end

    test "api_version and database" do
      plug = fn conn ->
        send(self(), {:path, conn.request_path})
        json_plug().(conn)
      end

      client = client_with_plug(plug, api_version: 1, database: "mydb")
      Client.get(client, "/_api/collection")
      assert_received {:path, "/_arango/v1/_db/mydb/_api/collection"}
    end

    test "database but no api_version" do
      plug = fn conn ->
        send(self(), {:path, conn.request_path})
        json_plug().(conn)
      end

      client = client_with_plug(plug, database: "mydb")
      Client.get(client, "/_api/collection")
      assert_received {:path, "/_db/mydb/_api/collection"}
    end
  end

  describe "HTTP methods" do
    test "get delegates to Req.get" do
      plug = fn conn ->
        assert conn.method == "GET"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, ~s({"ok":true}))
      end

      client = client_with_plug(plug)
      assert {:ok, %{status: 200, body: %{"ok" => true}}} = Client.get(client, "/_api/version")
    end

    test "post sends JSON body" do
      plug = fn conn ->
        assert conn.method == "POST"
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        send(self(), {:body, Jason.decode!(body)})

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(201, "{}")
      end

      client = client_with_plug(plug, content_type: :json)
      Client.post(client, "/_api/cursor", %{"query" => "RETURN 1"})
      assert_received {:body, %{"query" => "RETURN 1"}}
    end

    test "put sends JSON body" do
      plug = fn conn ->
        assert conn.method == "PUT"
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        send(self(), {:body, body})

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, "{}")
      end

      client = client_with_plug(plug, content_type: :json)
      Client.put(client, "/_api/cursor/123", %{"data" => true})
      assert_received {:body, body}
      assert %{"data" => true} = Jason.decode!(body)
    end

    test "put without body sends no JSON" do
      plug = fn conn ->
        assert conn.method == "PUT"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, "{}")
      end

      client = client_with_plug(plug)
      assert {:ok, %{status: 200}} = Client.put(client, "/_api/cursor/123")
    end

    test "delete delegates to Req.delete" do
      plug = fn conn ->
        assert conn.method == "DELETE"

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, "{}")
      end

      client = client_with_plug(plug)
      assert {:ok, %{status: 200}} = Client.delete(client, "/_api/collection/test")
    end
  end

  describe "request/5" do
    test "sends request with given method and path as-is (no prefix)" do
      plug = fn conn ->
        send(self(), {:request, conn.method, conn.request_path})
        json_plug().(conn)
      end

      client = client_with_plug(plug, api_version: 1, database: "mydb")
      Client.request(client, :post, "/_admin/echo")
      assert_received {:request, "POST", "/_admin/echo"}
    end

    test "vpack-encodes body by default" do
      plug = fn conn ->
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        send(self(), {:body, body})
        vpack_plug().(conn)
      end

      client = client_with_plug(plug)
      Client.request(client, :post, "/_api/cursor", %{"query" => "RETURN 1"})
      assert_received {:body, body}
      assert %{"query" => "RETURN 1"} = VelocyPack.decode!(body)
    end

    test "sends no body when body is nil" do
      plug = fn conn ->
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        send(self(), {:body, body})
        json_plug().(conn)
      end

      client = client_with_plug(plug)
      Client.request(client, :get, "/_api/version")
      assert_received {:body, ""}
    end

    test "applies auth headers" do
      plug = fn conn ->
        auth = Plug.Conn.get_req_header(conn, "authorization")
        send(self(), {:auth, auth})
        json_plug().(conn)
      end

      client = client_with_plug(plug) |> Client.with_auth({:basic, "root", "pass"})
      Client.request(client, :get, "/_api/version")
      expected = "Basic " <> Base.encode64("root:pass")
      assert_received {:auth, [^expected]}
    end

    test "passes extra opts through to Req" do
      plug = fn conn ->
        custom = Plug.Conn.get_req_header(conn, "x-custom")
        send(self(), {:custom, custom})
        json_plug().(conn)
      end

      client = client_with_plug(plug)
      Client.request(client, :get, "/_api/version", nil, headers: [{"x-custom", "value"}])
      assert_received {:custom, ["value"]}
    end
  end

  describe "vpack encoding" do
    test "sends body as vpack with correct content-type and accept headers" do
      plug = fn conn ->
        [content_type] = Plug.Conn.get_req_header(conn, "content-type")
        [accept] = Plug.Conn.get_req_header(conn, "accept")
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        send(self(), {:vpack_request, content_type, accept, body})
        vpack_plug().(conn)
      end

      client = client_with_plug(plug, content_type: :vpack)
      Client.post(client, "/_api/cursor", %{"query" => "RETURN 1"})

      assert_received {:vpack_request, content_type, accept, body}
      assert content_type == "application/x-velocypack"
      assert accept == "application/x-velocypack"
      assert %{"query" => "RETURN 1"} = VelocyPack.decode!(body)
    end

    test "sends no body when body is nil" do
      plug = fn conn ->
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        send(self(), {:body, body})
        vpack_plug().(conn)
      end

      client = client_with_plug(plug, content_type: :vpack)
      Client.get(client, "/_api/version")
      assert_received {:body, ""}
    end

    test "decodes vpack response body" do
      payload = %{"result" => [1], "hasMore" => false}

      plug = fn conn ->
        conn
        |> Plug.Conn.put_resp_content_type("application/x-velocypack")
        |> Plug.Conn.send_resp(200, VelocyPack.encode!(payload))
      end

      client = client_with_plug(plug, content_type: :vpack)
      assert {:ok, %{status: 200, body: body}} = Client.get(client, "/_api/cursor/123")
      assert body == payload
    end

    test "json client also decodes vpack response" do
      payload = %{"x" => 1}

      plug = fn conn ->
        conn
        |> Plug.Conn.put_resp_content_type("application/x-velocypack")
        |> Plug.Conn.send_resp(200, VelocyPack.encode!(payload))
      end

      client = client_with_plug(plug, content_type: :json)
      assert {:ok, %{status: 200, body: body}} = Client.get(client, "/_api/test")
      assert body == payload
    end
  end

  describe "protocol_connect_options/1" do
    test "returns empty list for :http1" do
      assert Client.protocol_connect_options(:http1) == []
    end

    test "returns http2 connect_options for :http2" do
      assert Client.protocol_connect_options(:http2) == [connect_options: [protocols: [:http2]]]
    end
  end

  describe "auth headers" do
    test "basic auth sets Authorization header" do
      plug = fn conn ->
        auth = Plug.Conn.get_req_header(conn, "authorization")
        send(self(), {:auth, auth})
        json_plug().(conn)
      end

      client = client_with_plug(plug) |> Client.with_auth({:basic, "root", "pass"})
      Client.get(client, "/_api/version")
      expected = "Basic " <> Base.encode64("root:pass")
      assert_received {:auth, [^expected]}
    end

    test "JWT auth sets Bearer header" do
      plug = fn conn ->
        auth = Plug.Conn.get_req_header(conn, "authorization")
        send(self(), {:auth, auth})
        json_plug().(conn)
      end

      client = client_with_plug(plug) |> Client.with_auth({:jwt, "my.jwt.token"})
      Client.get(client, "/_api/version")
      assert_received {:auth, ["Bearer my.jwt.token"]}
    end

    test "jwt_provider auth sets Bearer header with a freshly minted token per request" do
      plug = fn conn ->
        auth = Plug.Conn.get_req_header(conn, "authorization")
        send(self(), {:auth, auth})
        json_plug().(conn)
      end

      secret = Base.encode16(:crypto.strong_rand_bytes(32))
      provider = Toast.JWT.Provider.new({:hmac, secret})
      client = client_with_plug(plug) |> Client.with_auth({:jwt_provider, provider})

      Client.get(client, "/_api/version")
      assert_received {:auth, ["Bearer " <> token1]}

      # Second request re-mints — tokens include `iat` seconds so two rapid
      # requests may produce identical tokens, but the token must verify.
      Client.get(client, "/_api/version")
      assert_received {:auth, ["Bearer " <> token2]}

      signer = Joken.Signer.create("HS256", secret)
      assert {:ok, _} = Joken.verify_and_validate(%{}, token1, signer)
      assert {:ok, _} = Joken.verify_and_validate(%{}, token2, signer)
    end

    test "no auth sends no Authorization header" do
      plug = fn conn ->
        auth = Plug.Conn.get_req_header(conn, "authorization")
        send(self(), {:auth, auth})
        json_plug().(conn)
      end

      client = client_with_plug(plug)
      Client.get(client, "/_api/version")
      assert_received {:auth, []}
    end
  end

  describe "trx_id" do
    test "defaults to nil" do
      client = Client.new("http://localhost:8529")
      assert client.trx_id == nil
    end

    test "injects x-arango-trx-id header when trx_id is set" do
      plug = fn conn ->
        trx = Plug.Conn.get_req_header(conn, "x-arango-trx-id")
        send(self(), {:trx, trx})
        json_plug().(conn)
      end

      client = client_with_plug(plug)
      client = %{client | trx_id: "12345"}
      Client.get(client, "/_api/document/col")
      assert_received {:trx, ["12345"]}
    end

    test "does not inject header when trx_id is nil" do
      plug = fn conn ->
        trx = Plug.Conn.get_req_header(conn, "x-arango-trx-id")
        send(self(), {:trx, trx})
        json_plug().(conn)
      end

      client = client_with_plug(plug)
      Client.get(client, "/_api/document/col")
      assert_received {:trx, []}
    end
  end

  describe "delete with body" do
    test "sends body when :body opt is provided" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        {decoded, conn} = decode_request_body(conn)
        send(self(), {:body, decoded})

        send_encoded_response(conn, 200, %{})
      end

      client = client_with_plug(plug)
      Client.delete(client, "/_api/document/col", body: ["key1", "key2"])
      assert_received {:body, ["key1", "key2"]}
    end

    test "delete without :body opt sends no body" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        {:ok, raw, conn} = Plug.Conn.read_body(conn)
        send(self(), {:body, raw})

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(200, "{}")
      end

      client = client_with_plug(plug)
      Client.delete(client, "/_api/collection/test")
      assert_received {:body, ""}
    end
  end
end
