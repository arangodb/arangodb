defmodule Toast.Diagnostics.AgencyDumpTest do
  use ExUnit.Case, async: true

  import ExUnit.CaptureLog

  alias Toast.Diagnostics.AgencyDump

  @config_response %{"leaderId" => "AGNT-abc", "term" => 5}
  @state_response %{"current" => %{}, "log" => []}
  @plan_response [%{"arango" => %{"Plan" => %{}}}]

  defp agency_plug(conn) do
    case {conn.method, conn.request_path} do
      {"GET", "/_api/agency/config"} ->
        json_response(conn, 200, @config_response)

      {"GET", "/_api/agency/state"} ->
        json_response(conn, 200, @state_response)

      {"POST", "/_api/agency/read"} ->
        json_response(conn, 200, @plan_response)

      _ ->
        json_response(conn, 404, %{"error" => "not found"})
    end
  end

  defp failing_plug(conn) do
    json_response(conn, 503, %{"error" => "service unavailable"})
  end

  defp partial_plug(conn) do
    case {conn.method, conn.request_path} do
      {"GET", "/_api/agency/config"} ->
        json_response(conn, 200, @config_response)

      _ ->
        json_response(conn, 503, %{"error" => "service unavailable"})
    end
  end

  defp json_response(conn, status, body) do
    conn
    |> Plug.Conn.put_resp_content_type("application/json")
    |> Plug.Conn.send_resp(status, Jason.encode!(body))
  end

  defp agent(id), do: %{id: id, endpoint: "http://localhost:8529"}

  describe "struct" do
    test "has expected fields with nil defaults" do
      dump = %AgencyDump{}
      assert dump.agent_id == nil
      assert dump.config == nil
      assert dump.state == nil
      assert dump.plan == nil
      assert dump.error == nil
    end
  end

  describe "capture/1" do
    test "returns dump struct with all three fields populated" do
      result =
        AgencyDump.capture(
          agents: [agent("AGNT-1")],
          client_opts: [plug: &agency_plug/1]
        )

      assert %AgencyDump{} = result
      assert result.agent_id == "AGNT-1"
      assert result.config == @config_response
      assert result.state == @state_response
      assert result.plan == @plan_response
    end

    test "queries config, state, and plan endpoints" do
      test_pid = self()

      tracking_plug = fn conn ->
        send(test_pid, {:request, conn.method, conn.request_path})
        agency_plug(conn)
      end

      AgencyDump.capture(
        agents: [agent("AGNT-1")],
        client_opts: [plug: tracking_plug]
      )

      assert_received {:request, "GET", "/_api/agency/config"}
      assert_received {:request, "GET", "/_api/agency/state"}
      assert_received {:request, "POST", "/_api/agency/read"}
    end

    # T12: verify the POST body sent to /_api/agency/read
    test "sends [[/arango]] as POST body to agency read endpoint" do
      test_pid = self()

      body_tracking_plug = fn conn ->
        case {conn.method, conn.request_path} do
          {"POST", "/_api/agency/read"} ->
            {:ok, raw_body, conn} = Plug.Conn.read_body(conn)
            send(test_pid, {:post_body, Jason.decode!(raw_body)})
            json_response(conn, 200, @plan_response)

          _ ->
            agency_plug(conn)
        end
      end

      AgencyDump.capture(
        agents: [agent("AGNT-1")],
        client_opts: [plug: body_tracking_plug]
      )

      assert_received {:post_body, body}
      assert body == [["/arango"]]
    end

    test "tries next agent if first is unresponsive" do
      # First request (agent 1 config) returns 503, all subsequent requests succeed
      call_count = :counters.new(1, [:atomics])

      switching_plug = fn conn ->
        :counters.add(call_count, 1, 1)
        count = :counters.get(call_count, 1)

        if count <= 1 do
          failing_plug(conn)
        else
          agency_plug(conn)
        end
      end

      {result, log} =
        with_log(fn ->
          AgencyDump.capture(
            agents: [agent("AGNT-fail"), agent("AGNT-ok")],
            client_opts: [plug: switching_plug]
          )
        end)

      assert log =~ "AGNT-fail"
      assert %AgencyDump{agent_id: "AGNT-ok"} = result
      assert result.config == @config_response
    end

    test "returns nil with warning if no agents provided" do
      log =
        capture_log(fn ->
          result = AgencyDump.capture(agents: [])
          assert result == nil
        end)

      assert log =~ "no agents available"
    end

    test "returns nil if all agents fail" do
      log =
        capture_log(fn ->
          result =
            AgencyDump.capture(
              agents: [agent("AGNT-1"), agent("AGNT-2")],
              client_opts: [plug: &failing_plug/1]
            )

          assert result == nil
        end)

      assert log =~ "no responsive agents found"
    end

    test "fails if any sub-request returns non-200" do
      log =
        capture_log(fn ->
          result =
            AgencyDump.capture(
              agents: [agent("AGNT-1")],
              client_opts: [plug: &partial_plug/1]
            )

          assert result == nil
        end)

      assert log =~ "AGNT-1"
      assert log =~ "failed"
    end
  end
end
