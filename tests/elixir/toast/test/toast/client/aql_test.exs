defmodule Toast.Client.AQLTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  defp client_with_plug(plug) do
    Client.new("http://localhost:8529", plug: plug)
  end

  describe "execute/2" do
    test "sends POST to /_api/cursor with query" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/cursor"
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        decoded = Jason.decode!(body)
        assert decoded["query"] == "RETURN 1"
        assert decoded["bindVars"] == %{}

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(201, Jason.encode!(%{"result" => [1], "hasMore" => false}))
      end

      client = client_with_plug(plug)
      assert {:ok, [1]} = Client.AQL.execute(client, "RETURN 1")
    end

    test "passes bind_vars" do
      plug = fn conn ->
        {:ok, body, conn} = Plug.Conn.read_body(conn)
        decoded = Jason.decode!(body)
        assert decoded["bindVars"] == %{"x" => 42}

        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(201, Jason.encode!(%{"result" => [42], "hasMore" => false}))
      end

      client = client_with_plug(plug)
      assert {:ok, [42]} = Client.AQL.execute(client, "RETURN @x", %{"x" => 42})
    end

    test "cursor pagination follows hasMore" do
      call_count = :counters.new(1, [:atomics])

      plug = fn conn ->
        count = :counters.get(call_count, 1)
        :counters.add(call_count, 1, 1)

        case {conn.method, count} do
          {"POST", 0} ->
            body = Jason.encode!(%{"result" => [1, 2], "hasMore" => true, "id" => "cursor-1"})

            conn
            |> Plug.Conn.put_resp_content_type("application/json")
            |> Plug.Conn.send_resp(201, body)

          {"PUT", 1} ->
            assert conn.request_path == "/_api/cursor/cursor-1"
            body = Jason.encode!(%{"result" => [3, 4], "hasMore" => false})

            conn
            |> Plug.Conn.put_resp_content_type("application/json")
            |> Plug.Conn.send_resp(200, body)

          _ ->
            conn |> Plug.Conn.send_resp(500, "unexpected")
        end
      end

      client = client_with_plug(plug)
      assert {:ok, [1, 2, 3, 4]} = Client.AQL.execute(client, "FOR i IN 1..4 RETURN i")
    end
  end

  describe "execute!/2" do
    test "returns results directly on success" do
      plug = fn conn ->
        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(201, Jason.encode!(%{"result" => [1], "hasMore" => false}))
      end

      client = client_with_plug(plug)
      assert [1] = Client.AQL.execute!(client, "RETURN 1")
    end

    test "raises on error" do
      plug = fn conn ->
        conn
        |> Plug.Conn.put_resp_content_type("application/json")
        |> Plug.Conn.send_resp(400, Jason.encode!(%{"error" => true, "errorMessage" => "bad query"}))
      end

      client = client_with_plug(plug)

      assert_raise RuntimeError, ~r/AQL query failed/, fn ->
        Client.AQL.execute!(client, "INVALID")
      end
    end
  end
end
