defmodule Toast.Deployment.ClientTest.MockController do
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

  def handle_call({:get_servers, role}, _from, state) do
    filtered = Enum.filter(state.servers, &(&1.role == role))
    {:reply, filtered, state}
  end

  def handle_call(_msg, _from, state) do
    {:reply, :ok, state}
  end
end

defmodule Toast.Deployment.ClientTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment
  alias Toast.Deployment.ServerInstance
  alias Toast.Deployment.ClientTest.MockController

  defp server_instance(id, opts) do
    %ServerInstance{
      id: id,
      role: Keyword.get(opts, :role, :single),
      port: Keyword.get(opts, :port, 8529),
      endpoint: Keyword.get(opts, :endpoint, "http://localhost:8529")
    }
  end

  defp deployment(pid, mode \\ :single_server) do
    %Deployment{
      id: "test",
      mode: mode,
      config: %Toast.Config{},
      controller: pid,
      endpoint: "http://localhost:8529",
      work_dir: "/tmp"
    }
  end

  describe "client/2 with server_id (string)" do
    test "returns a client targeting that server's endpoint" do
      srv = server_instance("s1", endpoint: "http://localhost:9090")
      {:ok, pid} = MockController.start_link(servers: [srv])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:ok, client} = Deployment.client(deployment(pid), "s1")
      assert client.base_url == "http://localhost:9090"
    end

    test "returns {:error, :not_found} for unknown server_id" do
      {:ok, pid} = MockController.start_link(servers: [])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :not_found} = Deployment.client(deployment(pid), "nonexistent")
    end

    test "returns correct client when multiple servers exist" do
      srv1 = server_instance("coord-0", endpoint: "http://localhost:9001", role: :coordinator)
      srv2 = server_instance("db-0", endpoint: "http://localhost:9002", role: :dbserver)
      {:ok, pid} = MockController.start_link(servers: [srv1, srv2])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:ok, client} = Deployment.client(deployment(pid, :cluster), "db-0")
      assert client.base_url == "http://localhost:9002"
    end
  end

  describe "client/2 with role targeting (keyword list)" do
    test "returns client for first server of the given role" do
      srv1 = server_instance("coord-0", endpoint: "http://localhost:9001", role: :coordinator)
      srv2 = server_instance("coord-1", endpoint: "http://localhost:9002", role: :coordinator)
      srv3 = server_instance("db-0", endpoint: "http://localhost:9003", role: :dbserver)
      {:ok, pid} = MockController.start_link(servers: [srv1, srv2, srv3])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:ok, client} = Deployment.client(deployment(pid, :cluster), role: :coordinator)
      assert client.base_url == "http://localhost:9001"
    end

    test "role with index targets specific server" do
      srv1 = server_instance("coord-0", endpoint: "http://localhost:9001", role: :coordinator)
      srv2 = server_instance("coord-1", endpoint: "http://localhost:9002", role: :coordinator)
      {:ok, pid} = MockController.start_link(servers: [srv1, srv2])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:ok, client} =
               Deployment.client(deployment(pid, :cluster), role: :coordinator, index: 1)

      assert client.base_url == "http://localhost:9002"
    end

    test "returns {:error, :not_found} when no servers match role" do
      srv = server_instance("db-0", endpoint: "http://localhost:9001", role: :dbserver)
      {:ok, pid} = MockController.start_link(servers: [srv])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :not_found} =
               Deployment.client(deployment(pid, :cluster), role: :coordinator)
    end

    test "returns {:error, :not_found} when index out of range" do
      srv = server_instance("coord-0", endpoint: "http://localhost:9001", role: :coordinator)
      {:ok, pid} = MockController.start_link(servers: [srv])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :not_found} =
               Deployment.client(deployment(pid, :cluster), role: :coordinator, index: 5)
    end

    test "returns {:error, :invalid_target} when opts lack :role key" do
      {:ok, pid} = MockController.start_link(servers: [])

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :invalid_target} = Deployment.client(deployment(pid), foo: :bar)
    end
  end

  describe "client/2 with dead controller" do
    test "returns default error when controller is dead" do
      dead = spawn(fn -> :ok end)
      Process.sleep(50)

      # controller_call catches :exit and returns the default.
      # For server/2 the default is {:error, :stopped}, which client/2 propagates.
      assert {:error, _} = Deployment.client(deployment(dead), "s1")
    end

    test "role-based targeting returns default when controller is dead" do
      dead = spawn(fn -> :ok end)
      Process.sleep(50)

      # servers/2 with dead controller returns [] (the default),
      # so client/2 returns {:error, :not_found}
      assert {:error, :not_found} = Deployment.client(deployment(dead), role: :coordinator)
    end
  end
end
