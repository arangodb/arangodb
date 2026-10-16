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

defmodule Toast.Client.AQLTest do
  use ExUnit.Case, async: true

  alias Toast.Client

  import Toast.ClientTestHelpers

  describe "execute/2" do
    test "sends POST to /_api/cursor with query" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/cursor"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["query"] == "RETURN 1"
        assert decoded["bindVars"] == %{}

        send_encoded_response(conn, 201, %{"result" => [1], "hasMore" => false})
      end

      client = client_with_plug(plug)
      assert {:ok, [1]} = Client.AQL.execute(client, "RETURN 1")
    end

    test "passes bind_vars" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["bindVars"] == %{"x" => 42}

        send_encoded_response(conn, 201, %{"result" => [42], "hasMore" => false})
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
            send_encoded_response(conn, 201, %{
              "result" => [1, 2],
              "hasMore" => true,
              "id" => "cursor-1"
            })

          {"PUT", 1} ->
            assert conn.request_path == "/_api/cursor/cursor-1"

            send_encoded_response(conn, 200, %{"result" => [3, 4], "hasMore" => false})

          _ ->
            conn |> Plug.Conn.send_resp(500, "unexpected")
        end
      end

      client = client_with_plug(plug)
      assert {:ok, [1, 2, 3, 4]} = Client.AQL.execute(client, "FOR i IN 1..4 RETURN i")
    end

    test "cursor pagination preserves order across three pages" do
      call_count = :counters.new(1, [:atomics])

      plug = fn conn ->
        count = :counters.get(call_count, 1)
        :counters.add(call_count, 1, 1)

        case {conn.method, count} do
          {"POST", 0} ->
            send_encoded_response(conn, 201, %{
              "result" => [1, 2],
              "hasMore" => true,
              "id" => "cursor-1"
            })

          {"PUT", 1} ->
            send_encoded_response(conn, 200, %{
              "result" => [3, 4],
              "hasMore" => true,
              "id" => "cursor-1"
            })

          {"PUT", 2} ->
            send_encoded_response(conn, 200, %{"result" => [5, 6], "hasMore" => false})

          _ ->
            conn |> Plug.Conn.send_resp(500, "unexpected")
        end
      end

      client = client_with_plug(plug)
      assert {:ok, [1, 2, 3, 4, 5, 6]} = Client.AQL.execute(client, "FOR i IN 1..6 RETURN i")
    end
  end

  describe "execute!/2" do
    test "returns results directly on success" do
      plug = fn conn ->
        send_encoded_response(conn, 201, %{"result" => [1], "hasMore" => false})
      end

      client = client_with_plug(plug)
      assert [1] = Client.AQL.execute!(client, "RETURN 1")
    end

    test "raises on error" do
      plug = fn conn ->
        send_encoded_response(conn, 400, %{"error" => true, "errorMessage" => "bad query"})
      end

      client = client_with_plug(plug)

      assert_raise RuntimeError, ~r/AQL\.execute failed/, fn ->
        Client.AQL.execute!(client, "INVALID")
      end
    end
  end
end
