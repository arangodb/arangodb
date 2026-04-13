defmodule Toast.Client.TransactionTest do
  use ExUnit.Case, async: true

  alias Toast.Client
  alias Toast.Client.Transaction

  import Toast.ClientTestHelpers

  describe "begin/2" do
    test "sends POST to /_api/transaction/begin with collections and returns id" do
      plug = fn conn ->
        assert conn.method == "POST"
        assert conn.request_path == "/_api/transaction/begin"
        {decoded, conn} = decode_request_body(conn)
        assert decoded["collections"] == %{"write" => ["col"]}

        send_encoded_response(conn, 201, %{
          "result" => %{"id" => "trx-123", "status" => "running"}
        })
      end

      client = client_with_plug(plug)
      assert {:ok, "trx-123"} = Transaction.begin(client, %{write: ["col"]})
    end
  end

  describe "begin/3" do
    test "translates opts to camelCase body fields" do
      plug = fn conn ->
        {decoded, conn} = decode_request_body(conn)
        assert decoded["collections"] == %{"write" => ["col"]}
        assert decoded["waitForSync"] == true
        assert decoded["lockTimeout"] == 5
        assert decoded["maxTransactionSize"] == 1000
        assert decoded["allowImplicit"] == false

        send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-456"}})
      end

      client = client_with_plug(plug)

      assert {:ok, "trx-456"} =
               Transaction.begin(client, %{write: ["col"]},
                 wait_for_sync: true,
                 lock_timeout: 5,
                 max_transaction_size: 1000,
                 allow_implicit: false
               )
    end
  end

  describe "status/2" do
    test "sends GET to /_api/transaction/{id} and unwraps result" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/transaction/trx-123"

        send_encoded_response(conn, 200, %{
          "result" => %{"id" => "trx-123", "status" => "running"}
        })
      end

      client = client_with_plug(plug)

      assert {:ok, %{"id" => "trx-123", "status" => "running"}} =
               Transaction.status(client, "trx-123")
    end
  end

  describe "commit/2" do
    test "sends PUT to /_api/transaction/{id}" do
      plug = fn conn ->
        assert conn.method == "PUT"
        assert conn.request_path == "/_api/transaction/trx-123"

        send_encoded_response(conn, 200, %{
          "result" => %{"id" => "trx-123", "status" => "committed"}
        })
      end

      client = client_with_plug(plug)
      assert :ok = Transaction.commit(client, "trx-123")
    end
  end

  describe "abort/2" do
    test "sends DELETE to /_api/transaction/{id}" do
      plug = fn conn ->
        assert conn.method == "DELETE"
        assert conn.request_path == "/_api/transaction/trx-123"

        send_encoded_response(conn, 200, %{
          "result" => %{"id" => "trx-123", "status" => "aborted"}
        })
      end

      client = client_with_plug(plug)
      assert :ok = Transaction.abort(client, "trx-123")
    end
  end

  describe "list/1" do
    test "sends GET to /_api/transaction and unwraps transactions" do
      plug = fn conn ->
        assert conn.method == "GET"
        assert conn.request_path == "/_api/transaction"

        send_encoded_response(conn, 200, %{
          "transactions" => [%{"id" => "trx-1", "status" => "running"}]
        })
      end

      client = client_with_plug(plug)
      assert {:ok, [%{"id" => "trx-1"}]} = Transaction.list(client)
    end
  end

  describe "bind/2" do
    test "sets trx_id on client" do
      client = Client.new("http://localhost:8529")
      bound = Transaction.bind(client, "trx-123")
      assert bound.trx_id == "trx-123"
    end
  end

  describe "run/3" do
    test "begins, calls function with bound client, commits on {:ok, value}" do
      request_log = :ets.new(:request_log, [:ordered_set, :public])

      plug = fn conn ->
        counter = :ets.info(request_log, :size)
        :ets.insert(request_log, {counter, conn.method, conn.request_path})

        case {conn.method, conn.request_path} do
          {"POST", "/_api/transaction/begin"} ->
            send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-99"}})

          {"PUT", "/_api/transaction/trx-99"} ->
            send_encoded_response(conn, 200, %{"result" => %{"status" => "committed"}})

          _ ->
            send_encoded_response(conn, 200, %{})
        end
      end

      client = client_with_plug(plug)

      assert {:ok, :my_value} =
               Transaction.run(client, %{write: ["col"]}, fn trx_client ->
                 assert trx_client.trx_id == "trx-99"
                 {:ok, :my_value}
               end)

      # Verify: begin was called, then commit
      requests = :ets.tab2list(request_log) |> Enum.map(fn {_, m, p} -> {m, p} end)
      assert {"POST", "/_api/transaction/begin"} in requests
      assert {"PUT", "/_api/transaction/trx-99"} in requests
      :ets.delete(request_log)
    end

    test "begins, aborts on {:error, reason}, returns the error" do
      plug = fn conn ->
        case {conn.method, conn.request_path} do
          {"POST", "/_api/transaction/begin"} ->
            send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-err"}})

          {"DELETE", "/_api/transaction/trx-err"} ->
            send_encoded_response(conn, 200, %{"result" => %{"status" => "aborted"}})

          _ ->
            send_encoded_response(conn, 200, %{})
        end
      end

      client = client_with_plug(plug)

      assert {:error, :some_reason} =
               Transaction.run(client, %{write: ["col"]}, fn _trx_client ->
                 {:error, :some_reason}
               end)
    end

    test "begins, aborts and re-raises on exception" do
      plug = fn conn ->
        case {conn.method, conn.request_path} do
          {"POST", "/_api/transaction/begin"} ->
            send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-raise"}})

          {"DELETE", "/_api/transaction/trx-raise"} ->
            send_encoded_response(conn, 200, %{"result" => %{"status" => "aborted"}})

          _ ->
            send_encoded_response(conn, 200, %{})
        end
      end

      client = client_with_plug(plug)

      assert_raise RuntimeError, "boom", fn ->
        Transaction.run(client, %{write: ["col"]}, fn _trx_client ->
          raise "boom"
        end)
      end
    end

    test "begins, aborts and raises ArgumentError on invalid return" do
      plug = fn conn ->
        case {conn.method, conn.request_path} do
          {"POST", "/_api/transaction/begin"} ->
            send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-bad"}})

          {"DELETE", "/_api/transaction/trx-bad"} ->
            send_encoded_response(conn, 200, %{"result" => %{"status" => "aborted"}})

          _ ->
            send_encoded_response(conn, 200, %{})
        end
      end

      client = client_with_plug(plug)

      assert_raise ArgumentError, ~r/expected \{:ok, _\} or \{:error, _\}/, fn ->
        Transaction.run(client, %{write: ["col"]}, fn _trx_client ->
          :something_else
        end)
      end
    end

    test "returns commit error with phase info when commit fails" do
      plug = fn conn ->
        case {conn.method, conn.request_path} do
          {"POST", "/_api/transaction/begin"} ->
            send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-cf"}})

          {"PUT", "/_api/transaction/trx-cf"} ->
            send_encoded_response(conn, 500, %{"error" => true, "errorMessage" => "commit failed"})

          {"DELETE", "/_api/transaction/trx-cf"} ->
            send_encoded_response(conn, 200, %{"result" => %{"status" => "aborted"}})

          _ ->
            send_encoded_response(conn, 200, %{})
        end
      end

      client = client_with_plug(plug)

      assert {:error, %{phase: :commit, reason: _}} =
               Transaction.run(client, %{write: ["col"]}, fn _trx_client ->
                 {:ok, :value}
               end)
    end

    test "passes opts through to begin" do
      plug = fn conn ->
        case {conn.method, conn.request_path} do
          {"POST", "/_api/transaction/begin"} ->
            {decoded, conn} = decode_request_body(conn)
            assert decoded["waitForSync"] == true

            send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-opts"}})

          {"PUT", "/_api/transaction/trx-opts"} ->
            send_encoded_response(conn, 200, %{"result" => %{"status" => "committed"}})

          _ ->
            send_encoded_response(conn, 200, %{})
        end
      end

      client = client_with_plug(plug)

      assert {:ok, :done} =
               Transaction.run(client, %{write: ["col"]}, [wait_for_sync: true], fn _trx_client ->
                 {:ok, :done}
               end)
    end
  end

  describe "bang variants" do
    test "begin! returns id on success" do
      plug = fn conn ->
        send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-b"}})
      end

      client = client_with_plug(plug)
      assert "trx-b" = Transaction.begin!(client, %{write: ["col"]})
    end

    test "begin! raises on error" do
      client = client_with_plug(json_plug(500, %{"error" => true}))

      assert_raise RuntimeError, ~r/Transaction\.begin failed/, fn ->
        Transaction.begin!(client, %{write: ["col"]})
      end
    end

    test "commit! returns :ok on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => %{"status" => "committed"}})
      end

      client = client_with_plug(plug)
      assert :ok = Transaction.commit!(client, "trx-123")
    end

    test "abort! returns :ok on success" do
      plug = fn conn ->
        send_encoded_response(conn, 200, %{"result" => %{"status" => "aborted"}})
      end

      client = client_with_plug(plug)
      assert :ok = Transaction.abort!(client, "trx-123")
    end

    test "run! returns value on success" do
      plug = fn conn ->
        case {conn.method, conn.request_path} do
          {"POST", "/_api/transaction/begin"} ->
            send_encoded_response(conn, 201, %{"result" => %{"id" => "trx-r"}})

          {"PUT", "/_api/transaction/trx-r"} ->
            send_encoded_response(conn, 200, %{"result" => %{"status" => "committed"}})

          _ ->
            send_encoded_response(conn, 200, %{})
        end
      end

      client = client_with_plug(plug)

      assert :val =
               Transaction.run!(client, %{write: ["col"]}, fn _trx_client ->
                 {:ok, :val}
               end)
    end
  end
end
