defmodule Toast.Deployment.ServerControlTest.MockController do
  @moduledoc false
  use GenServer

  def start_link(opts \\ []) do
    response = Keyword.get(opts, :response, :ok)
    GenServer.start_link(__MODULE__, %{calls: [], response: response})
  end

  def calls(pid), do: GenServer.call(pid, :get_calls)

  @impl true
  def init(state), do: {:ok, state}

  @impl true
  def handle_call(:get_calls, _from, state) do
    {:reply, Enum.reverse(state.calls), state}
  end

  def handle_call(msg, _from, state) do
    {:reply, state.response, %{state | calls: [msg | state.calls]}}
  end
end

defmodule Toast.Deployment.ServerControlTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment
  alias Toast.Deployment.ServerControlTest.MockController

  defp deployment(pid) do
    %Deployment{
      id: "test",
      controller: pid
    }
  end

  describe "operation delegation" do
    setup do
      {:ok, pid} = MockController.start_link()

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      %{pid: pid}
    end

    test "stop_server sends {:stop_server, target}", %{pid: pid} do
      assert :ok = Deployment.stop_server(deployment(pid), "s1")
      assert [{:stop_server, "s1"}] = MockController.calls(pid)
    end

    test "kill_server sends {:kill_server, target}", %{pid: pid} do
      assert :ok = Deployment.kill_server(deployment(pid), "s1")
      assert [{:kill_server, "s1"}] = MockController.calls(pid)
    end

    test "pause_server sends {:pause_server, target}", %{pid: pid} do
      assert :ok = Deployment.pause_server(deployment(pid), "s1")
      assert [{:pause_server, "s1"}] = MockController.calls(pid)
    end

    test "resume_server sends {:resume_server, target}", %{pid: pid} do
      assert :ok = Deployment.resume_server(deployment(pid), "s1")
      assert [{:resume_server, "s1"}] = MockController.calls(pid)
    end

    test "restart_server without opts sends {:restart_server, target, []}", %{pid: pid} do
      assert :ok = Deployment.restart_server(deployment(pid), "s1")
      assert [{:restart_server, "s1", []}] = MockController.calls(pid)
    end

    test "start_server without opts sends {:start_server, target, []}", %{pid: pid} do
      assert :ok = Deployment.start_server(deployment(pid), "s1")
      assert [{:start_server, "s1", []}] = MockController.calls(pid)
    end
  end

  describe "opts passthrough" do
    setup do
      {:ok, pid} = MockController.start_link()

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      %{pid: pid}
    end

    test "restart_server forwards opts to controller", %{pid: pid} do
      opts = [clean: true, timeout: 30_000]
      assert :ok = Deployment.restart_server(deployment(pid), "s1", opts)
      assert [{:restart_server, "s1", ^opts}] = MockController.calls(pid)
    end

    test "start_server forwards opts to controller", %{pid: pid} do
      opts = [extra_args: ["--log.level", "debug"]]
      assert :ok = Deployment.start_server(deployment(pid), "s1", opts)
      assert [{:start_server, "s1", ^opts}] = MockController.calls(pid)
    end
  end

  describe "dead controller" do
    test "returns {:error, :controller_not_available}" do
      dead = spawn(fn -> :ok end)
      Process.sleep(50)
      assert {:error, :controller_not_available} = Deployment.stop_server(deployment(dead), "s1")
    end
  end
end
