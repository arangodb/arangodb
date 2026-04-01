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

    test "passes remaining opts to Req" do
      client = Client.new("http://localhost:8529", receive_timeout: 5_000)
      assert client.req.options.receive_timeout == 5_000
    end

    test "disables retry by default" do
      client = Client.new("http://localhost:8529")
      assert client.req.options.retry == false
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

      client = client_with_plug(plug)
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

      client = client_with_plug(plug)
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
end
