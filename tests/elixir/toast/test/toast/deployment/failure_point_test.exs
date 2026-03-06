defmodule Toast.Deployment.FailurePointTest.MockController do
  @moduledoc false
  use GenServer

  def start_link(opts \\ []) do
    servers = Keyword.get(opts, :servers, [])
    GenServer.start_link(__MODULE__, %{servers: servers})
  end

  @impl true
  def init(state), do: {:ok, state}

  @impl true
  def handle_call({:get_server, server_id}, _from, state) do
    case Enum.find(state.servers, &(&1.id == server_id)) do
      nil -> {:reply, {:error, :not_found}, state}
      srv -> {:reply, srv, state}
    end
  end

  def handle_call(:get_servers, _from, state) do
    {:reply, state.servers, state}
  end

  def handle_call(_msg, _from, state) do
    {:reply, :ok, state}
  end
end

defmodule Toast.Deployment.FailurePointTest do
  use ExUnit.Case, async: false

  alias Toast.Client
  alias Toast.Deployment
  alias Toast.Deployment.FailurePoint
  alias Toast.Deployment.FailurePointTest.MockController

  defp client_with_plug(plug) do
    Client.new("http://localhost:8529", plug: plug)
  end

  defp json_plug(status, body \\ %{}) do
    fn conn ->
      conn
      |> Plug.Conn.put_resp_content_type("application/json")
      |> Plug.Conn.send_resp(status, Jason.encode!(body))
    end
  end

  defp deployment(pid, mode \\ :single_server) do
    %Deployment{
      id: "test",
      mode: mode,
      config: %Toast.Config{},
      controller: pid,
      endpoint: "http://localhost:1"
    }
  end

  # --- do_set/2: PUT /_admin/debug/failat/{name} ---

  describe "do_set/2" do
    test "sends PUT to /_admin/debug/failat/{name}" do
      test_pid = self()

      plug = fn conn ->
        send(test_pid, {:request, conn.method, conn.request_path})
        json_plug(200).(conn)
      end

      client = client_with_plug(plug)
      assert :ok = FailurePoint.do_set(client, "failpoints::myFailure")
      assert_received {:request, "PUT", "/_admin/debug/failat/failpoints::myFailure"}
    end

    test "returns :ok on HTTP 200" do
      client = client_with_plug(json_plug(200))
      assert :ok = FailurePoint.do_set(client, "some-failure")
    end

    test "returns {:error, :not_supported} on HTTP 404" do
      client = client_with_plug(json_plug(404))
      assert {:error, :not_supported} = FailurePoint.do_set(client, "some-failure")
    end

    test "returns {:error, {:unexpected_status, status}} on other HTTP errors" do
      client = client_with_plug(json_plug(500))
      assert {:error, {:unexpected_status, 500}} = FailurePoint.do_set(client, "some-failure")
    end
  end

  # --- do_clear/2: DELETE /_admin/debug/failat/{name} ---

  describe "do_clear/2" do
    test "sends DELETE to /_admin/debug/failat/{name}" do
      test_pid = self()

      plug = fn conn ->
        send(test_pid, {:request, conn.method, conn.request_path})
        json_plug(200).(conn)
      end

      client = client_with_plug(plug)
      assert :ok = FailurePoint.do_clear(client, "failpoints::myFailure")
      assert_received {:request, "DELETE", "/_admin/debug/failat/failpoints::myFailure"}
    end

    test "returns :ok on HTTP 200" do
      client = client_with_plug(json_plug(200))
      assert :ok = FailurePoint.do_clear(client, "some-failure")
    end

    test "returns {:error, :not_supported} on HTTP 404" do
      client = client_with_plug(json_plug(404))
      assert {:error, :not_supported} = FailurePoint.do_clear(client, "some-failure")
    end

    test "returns {:error, {:unexpected_status, status}} on other HTTP errors" do
      client = client_with_plug(json_plug(500))
      assert {:error, {:unexpected_status, 500}} = FailurePoint.do_clear(client, "some-failure")
    end
  end

  # --- do_clear_all/1: DELETE /_admin/debug/failat ---

  describe "do_clear_all/1" do
    test "sends DELETE to /_admin/debug/failat (no name suffix)" do
      test_pid = self()

      plug = fn conn ->
        send(test_pid, {:request, conn.method, conn.request_path})
        json_plug(200).(conn)
      end

      client = client_with_plug(plug)
      assert :ok = FailurePoint.do_clear_all(client)
      assert_received {:request, "DELETE", "/_admin/debug/failat"}
    end

    test "returns :ok on HTTP 200" do
      client = client_with_plug(json_plug(200))
      assert :ok = FailurePoint.do_clear_all(client)
    end

    test "returns {:error, :not_supported} on HTTP 404" do
      client = client_with_plug(json_plug(404))
      assert {:error, :not_supported} = FailurePoint.do_clear_all(client)
    end
  end

  # --- set/3: deployment-level target resolution ---

  describe "set/3" do
    test "returns {:error, :not_found} for unknown server_id" do
      {:ok, pid} = MockController.start_link(servers: [])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :not_found} = FailurePoint.set(deployment(pid), "nonexistent", "test")
    end
  end

  # --- clear/3: deployment-level target resolution ---

  describe "clear/3" do
    test "returns {:error, :not_found} for unknown server_id" do
      {:ok, pid} = MockController.start_link(servers: [])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :not_found} = FailurePoint.clear(deployment(pid), "nonexistent", "test")
    end
  end

  # --- clear_all/1: iterates all servers ---

  describe "clear_all/1" do
    test "returns :ok when deployment has no servers" do
      {:ok, pid} = MockController.start_link(servers: [])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert :ok = FailurePoint.clear_all(deployment(pid))
    end
  end

  # --- Dead controller ---

  describe "dead controller" do
    setup do
      dead = spawn(fn -> :ok end)
      Process.sleep(50)
      %{deployment: deployment(dead)}
    end

    test "set/3 returns error", %{deployment: d} do
      assert {:error, _} = FailurePoint.set(d, "s1", "test")
    end

    test "clear/3 returns error", %{deployment: d} do
      assert {:error, _} = FailurePoint.clear(d, "s1", "test")
    end

    test "clear_all/1 returns :ok (servers/1 defaults to [] for dead controller)", %{
      deployment: d
    } do
      assert :ok = FailurePoint.clear_all(d)
    end
  end

  # --- Deployment delegates ---

  describe "Deployment delegates" do
    setup do
      {:ok, pid} = MockController.start_link(servers: [])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      %{deployment: deployment(pid)}
    end

    test "set_failure_point/3 delegates to FailurePoint.set/3", %{deployment: d} do
      assert FailurePoint.set(d, "s1", "fp") == Deployment.set_failure_point(d, "s1", "fp")
    end

    test "clear_failure_point/3 delegates to FailurePoint.clear/3", %{deployment: d} do
      assert FailurePoint.clear(d, "s1", "fp") == Deployment.clear_failure_point(d, "s1", "fp")
    end

    test "clear_all_failure_points/1 delegates to FailurePoint.clear_all/1", %{deployment: d} do
      assert FailurePoint.clear_all(d) == Deployment.clear_all_failure_points(d)
    end
  end
end
