defmodule Toast.Diagnostics.AgencyDumpTest do
  use ExUnit.Case, async: true

  import ExUnit.CaptureLog

  alias Toast.Diagnostics.AgencyDump

  @dump_response %{"agency" => %{"arango" => %{"Plan" => %{}}}}

  defp agent_plug(conn) do
    case {conn.method, conn.request_path} do
      {"GET", "/_api/agency/state"} ->
        json_response(conn, 200, @dump_response)

      _ ->
        json_response(conn, 404, %{"error" => "not found"})
    end
  end

  defp failing_plug(conn) do
    json_response(conn, 503, %{"error" => "service unavailable"})
  end

  defp json_response(conn, status, body) do
    conn
    |> Plug.Conn.put_resp_content_type("application/json")
    |> Plug.Conn.send_resp(status, Jason.encode!(body))
  end

  describe "capture/1" do
    test "returns raw JSON from agent" do
      result =
        AgencyDump.capture(
          endpoints: ["http://agent:8531"],
          client_opts: [plug: &agent_plug/1]
        )

      assert is_binary(result)
      assert Jason.decode!(result) == @dump_response
    end

    test "hits /_api/agency/state" do
      test_pid = self()

      tracking_plug = fn conn ->
        send(test_pid, {:request, conn.method, conn.request_path})
        agent_plug(conn)
      end

      AgencyDump.capture(
        endpoints: ["http://agent:8531"],
        client_opts: [plug: tracking_plug]
      )

      assert_received {:request, "GET", "/_api/agency/state"}
    end

    test "tries next agent if first fails" do
      call_count = :counters.new(1, [:atomics])

      plug = fn conn ->
        :counters.add(call_count, 1, 1)

        if :counters.get(call_count, 1) <= 1 do
          failing_plug(conn)
        else
          agent_plug(conn)
        end
      end

      {result, log} =
        with_log(fn ->
          AgencyDump.capture(
            endpoints: ["http://agent-1:8531", "http://agent-2:8531"],
            client_opts: [plug: plug]
          )
        end)

      assert log =~ "agent-1"
      assert is_binary(result)
      assert Jason.decode!(result) == @dump_response
    end

    test "returns nil when no endpoints provided" do
      log =
        capture_log(fn ->
          assert AgencyDump.capture(endpoints: []) == nil
        end)

      assert log =~ "no agent endpoints"
    end

    test "sends auth header when auth option provided" do
      test_pid = self()

      auth_checking_plug = fn conn ->
        auth = Plug.Conn.get_req_header(conn, "authorization")
        send(test_pid, {:auth_header, auth})
        agent_plug(conn)
      end

      AgencyDump.capture(
        endpoints: ["http://agent:8531"],
        auth: {:jwt, "test-token-123"},
        client_opts: [plug: auth_checking_plug]
      )

      assert_received {:auth_header, ["Bearer test-token-123"]}
    end

    test "sends no auth header when auth option is nil" do
      test_pid = self()

      auth_checking_plug = fn conn ->
        auth = Plug.Conn.get_req_header(conn, "authorization")
        send(test_pid, {:auth_header, auth})
        agent_plug(conn)
      end

      AgencyDump.capture(
        endpoints: ["http://agent:8531"],
        client_opts: [plug: auth_checking_plug]
      )

      assert_received {:auth_header, []}
    end

    test "returns nil if all agents fail" do
      log =
        capture_log(fn ->
          result =
            AgencyDump.capture(
              endpoints: ["http://agent-1:8531", "http://agent-2:8531"],
              client_opts: [plug: &failing_plug/1]
            )

          assert result == nil
        end)

      assert log =~ "no agent responded"
    end
  end

  describe "write/3" do
    @tag :tmp_dir
    test "writes JSON file to directory", %{tmp_dir: tmp_dir} do
      json = Jason.encode!(@dump_response)

      assert {:ok, path} = AgencyDump.write(json, tmp_dir, "cluster-1")
      assert path == Path.join(tmp_dir, "agency-dump-cluster-1.json")
      assert File.exists?(path)
      assert Jason.decode!(File.read!(path)) == @dump_response
    end

    @tag :tmp_dir
    test "creates directory if it does not exist", %{tmp_dir: tmp_dir} do
      dir = Path.join(tmp_dir, "nested/subdir")

      assert {:ok, _path} = AgencyDump.write("{}", dir, "d1")
      assert File.exists?(Path.join(dir, "agency-dump-d1.json"))
    end

    @tag :tmp_dir
    test "accepts iodata", %{tmp_dir: tmp_dir} do
      json = ["{", ~s("key":), ~s("value"), "}"]

      assert {:ok, path} = AgencyDump.write(json, tmp_dir, "d1")
      assert File.read!(path) == ~s({"key":"value"})
    end
  end
end
