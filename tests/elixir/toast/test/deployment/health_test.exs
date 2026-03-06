defmodule Toast.Deployment.HealthTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.Health

  defmodule HttpServer do
    @moduledoc false

    def start(handler) do
      {:ok, listen} = :gen_tcp.listen(0, [:binary, active: false, reuseaddr: true])
      {:ok, port} = :inet.port(listen)
      pid = spawn_link(fn -> accept_loop(listen, handler) end)
      {:ok, pid, port}
    end

    defp accept_loop(listen, handler) do
      case :gen_tcp.accept(listen, 100) do
        {:ok, socket} ->
          spawn(fn -> handle_connection(socket, handler) end)
          accept_loop(listen, handler)

        {:error, :timeout} ->
          accept_loop(listen, handler)

        {:error, :closed} ->
          :ok
      end
    end

    defp handle_connection(socket, handler) do
      case :gen_tcp.recv(socket, 0, 5_000) do
        {:ok, data} ->
          {path, _} = parse_request_path(data)
          {status, body} = handler.(path)
          response = build_response(status, body)
          :gen_tcp.send(socket, response)
          :gen_tcp.close(socket)

        {:error, _} ->
          :gen_tcp.close(socket)
      end
    end

    defp parse_request_path(data) do
      [request_line | _] = String.split(data, "\r\n", parts: 2)
      [_method, path | _] = String.split(request_line, " ")
      {path, data}
    end

    defp build_response(status, body) do
      json = Jason.encode!(body)
      reason = status_reason(status)

      "HTTP/1.1 #{status} #{reason}\r\n" <>
        "content-type: application/json\r\n" <>
        "content-length: #{byte_size(json)}\r\n" <>
        "\r\n" <>
        json
    end

    defp status_reason(200), do: "OK"
    defp status_reason(503), do: "Service Unavailable"
    defp status_reason(_), do: "Error"
  end

  defp start_server(handler) do
    {:ok, pid, port} = HttpServer.start(handler)
    on_cleanup(fn -> Process.exit(pid, :kill) end)
    {"http://127.0.0.1:#{port}", pid}
  end

  defp on_cleanup(fun) do
    ExUnit.Callbacks.on_exit(fun)
  end

  describe "check_once/2" do
    test "returns :ok when server responds with 200" do
      {endpoint, _} =
        start_server(fn _path ->
          {200, %{"server" => "arango", "version" => "3.12.0"}}
        end)

      assert :ok = Health.check_once(endpoint, http_timeout: 2_000)
    end

    test "returns error for non-2xx status" do
      {endpoint, _} =
        start_server(fn _path ->
          {503, %{"error" => true}}
        end)

      assert {:error, {:unexpected_status, 503}} =
               Health.check_once(endpoint, http_timeout: 2_000)
    end

    test "returns error when connection refused" do
      assert {:error, _reason} = Health.check_once("http://127.0.0.1:1", http_timeout: 500)
    end

    test "hits /_api/version endpoint" do
      test_pid = self()

      {endpoint, _} =
        start_server(fn path ->
          send(test_pid, {:path, path})
          {200, %{}}
        end)

      Health.check_once(endpoint, http_timeout: 2_000)
      assert_received {:path, "/_api/version"}
    end
  end

  describe "wait_until_ready/2" do
    test "returns :ok when server is immediately ready" do
      {endpoint, _} =
        start_server(fn _path ->
          {200, %{"version" => "3.12.0"}}
        end)

      assert :ok = Health.wait_until_ready(endpoint, timeout: 2_000, poll_interval: 50)
    end

    test "returns timeout error when server never becomes ready" do
      {endpoint, _} =
        start_server(fn _path ->
          {503, %{"error" => true}}
        end)

      assert {:error, :timeout} =
               Health.wait_until_ready(endpoint, timeout: 200, poll_interval: 50)
    end

    test "returns timeout error when connection always refused" do
      assert {:error, :timeout} =
               Health.wait_until_ready("http://127.0.0.1:1",
                 timeout: 200,
                 poll_interval: 50,
                 http_timeout: 50
               )
    end

    test "returns process_died when process_check_fn returns false" do
      {endpoint, _} =
        start_server(fn _path ->
          {503, %{"error" => true}}
        end)

      assert {:error, :process_died} =
               Health.wait_until_ready(endpoint,
                 timeout: 2_000,
                 poll_interval: 50,
                 process_check_fn: fn -> false end
               )
    end

    test "polls until server becomes ready" do
      call_count = :counters.new(1, [:atomics])

      {endpoint, _} =
        start_server(fn _path ->
          :counters.add(call_count, 1, 1)

          if :counters.get(call_count, 1) >= 3 do
            {200, %{"version" => "3.12.0"}}
          else
            {503, %{"error" => true}}
          end
        end)

      assert :ok = Health.wait_until_ready(endpoint, timeout: 5_000, poll_interval: 50)
      assert :counters.get(call_count, 1) >= 3
    end
  end

  describe "wait_for_agency_ready/2" do
    test "returns :ok when all agents agree on leader" do
      config = %{"leaderId" => "AGNT-abc", "lastAcked" => %{}}

      {endpoint, _} =
        start_server(fn _path ->
          {200, config}
        end)

      assert :ok =
               Health.wait_for_agency_ready([endpoint],
                 timeout: 2_000,
                 poll_interval: 50
               )
    end

    test "returns :ok with multiple agents agreeing on leader" do
      config = %{"leaderId" => "AGNT-abc", "lastAcked" => %{}}

      endpoints =
        for _ <- 1..3 do
          {endpoint, _} =
            start_server(fn _path ->
              {200, config}
            end)

          endpoint
        end

      assert :ok =
               Health.wait_for_agency_ready(endpoints,
                 timeout: 2_000,
                 poll_interval: 50
               )
    end

    test "times out when agents disagree on leader" do
      configs = [
        %{"leaderId" => "AGNT-1", "lastAcked" => %{}},
        %{"leaderId" => "AGNT-2", "lastAcked" => %{}}
      ]

      endpoints =
        for config <- configs do
          {endpoint, _} =
            start_server(fn _path ->
              {200, config}
            end)

          endpoint
        end

      assert {:error, :timeout} =
               Health.wait_for_agency_ready(endpoints,
                 timeout: 200,
                 poll_interval: 50
               )
    end

    test "times out when agents missing leader ID" do
      config = %{"lastAcked" => %{}}

      {endpoint, _} =
        start_server(fn _path ->
          {200, config}
        end)

      assert {:error, :timeout} =
               Health.wait_for_agency_ready([endpoint],
                 timeout: 200,
                 poll_interval: 50
               )
    end

    test "times out when no lastAcked field present" do
      config = %{"leaderId" => "AGNT-abc"}

      {endpoint, _} =
        start_server(fn _path ->
          {200, config}
        end)

      assert {:error, :timeout} =
               Health.wait_for_agency_ready([endpoint],
                 timeout: 200,
                 poll_interval: 50
               )
    end

    test "times out when agents are not responding" do
      assert {:error, :timeout} =
               Health.wait_for_agency_ready(["http://127.0.0.1:1"],
                 timeout: 200,
                 poll_interval: 50,
                 http_timeout: 50
               )
    end

    test "hits /_api/agency/config endpoint" do
      test_pid = self()
      config = %{"leaderId" => "AGNT-abc", "lastAcked" => %{}}

      {endpoint, _} =
        start_server(fn path ->
          send(test_pid, {:agency_path, path})
          {200, config}
        end)

      Health.wait_for_agency_ready([endpoint], timeout: 2_000, poll_interval: 50)
      assert_received {:agency_path, "/_api/agency/config"}
    end

    test "succeeds once agents reach consensus" do
      call_count = :counters.new(1, [:atomics])

      {endpoint, _} =
        start_server(fn _path ->
          :counters.add(call_count, 1, 1)

          if :counters.get(call_count, 1) >= 3 do
            {200, %{"leaderId" => "AGNT-abc", "lastAcked" => %{}}}
          else
            {200, %{"leaderId" => ""}}
          end
        end)

      assert :ok =
               Health.wait_for_agency_ready([endpoint],
                 timeout: 5_000,
                 poll_interval: 50
               )
    end
  end
end
