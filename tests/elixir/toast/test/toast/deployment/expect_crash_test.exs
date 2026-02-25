defmodule Toast.Deployment.ExpectCrashTest.MockController do
  @moduledoc false
  use GenServer

  def start_link(opts \\ []) do
    responses = Keyword.get(opts, :responses, %{})
    GenServer.start_link(__MODULE__, %{calls: [], responses: responses})
  end

  def calls(pid), do: GenServer.call(pid, :get_calls)

  @impl true
  def init(state), do: {:ok, state}

  @impl true
  def handle_call(:get_calls, _from, state) do
    {:reply, Enum.reverse(state.calls), state}
  end

  def handle_call(msg, _from, state) do
    response = lookup_response(state.responses, msg)
    {:reply, response, %{state | calls: [msg | state.calls]}}
  end

  defp lookup_response(responses, {:expect_crash, _server_id, _timeout}) do
    Map.get(responses, :expect_crash, :ok)
  end

  defp lookup_response(responses, {:verify_crash, _server_id, _timeout}) do
    Map.get(responses, :verify_crash, {:ok, %{exit_status: 11, signal: 11, timestamp: ~U[2026-01-01 00:00:00Z]}})
  end

  defp lookup_response(_responses, _msg), do: :ok
end

defmodule Toast.Deployment.ExpectCrashTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment
  alias Toast.Deployment.ExpectCrashTest.MockController

  defp deployment(pid, mode \\ :single_server) do
    %Deployment{
      id: "test",
      mode: mode,
      config: %Toast.Config{},
      controller: pid,
      endpoint: "http://localhost:1",
      work_dir: "/tmp"
    }
  end

  describe "expect_crash delegation" do
    setup do
      {:ok, pid} = MockController.start_link()
      on_exit(fn -> if Process.alive?(pid), do: GenServer.stop(pid) end)
      %{pid: pid}
    end

    test "sends {:expect_crash, server_id, timeout} to controller", %{pid: pid} do
      assert :ok = Deployment.expect_crash(deployment(pid), "s1")
      assert [{:expect_crash, "s1", 30_000}] = MockController.calls(pid)
    end

    test "default timeout is 30_000", %{pid: pid} do
      Deployment.expect_crash(deployment(pid), "s1")
      [{:expect_crash, _server_id, timeout}] = MockController.calls(pid)
      assert timeout == 30_000
    end

    test "custom timeout passes through", %{pid: pid} do
      assert :ok = Deployment.expect_crash(deployment(pid), "s1", timeout: 60_000)
      assert [{:expect_crash, "s1", 60_000}] = MockController.calls(pid)
    end

    test "returns {:error, :already_expected} from controller", %{pid: pid} do
      {:ok, pid} = MockController.start_link(responses: %{expect_crash: {:error, :already_expected}})
      on_exit(fn -> if Process.alive?(pid), do: GenServer.stop(pid) end)

      assert {:error, :already_expected} = Deployment.expect_crash(deployment(pid), "s1")
    end
  end

  describe "verify_crash delegation" do
    setup do
      {:ok, pid} = MockController.start_link()
      on_exit(fn -> if Process.alive?(pid), do: GenServer.stop(pid) end)
      %{pid: pid}
    end

    test "sends {:verify_crash, server_id, timeout} to controller", %{pid: pid} do
      assert {:ok, _crash_info} = Deployment.verify_crash(deployment(pid), "s1")
      assert [{:verify_crash, "s1", 5_000}] = MockController.calls(pid)
    end

    test "default timeout is 5_000", %{pid: pid} do
      Deployment.verify_crash(deployment(pid), "s1")
      [{:verify_crash, _server_id, timeout}] = MockController.calls(pid)
      assert timeout == 5_000
    end

    test "custom timeout passes through", %{pid: pid} do
      Deployment.verify_crash(deployment(pid), "s1", timeout: 15_000)
      assert [{:verify_crash, "s1", 15_000}] = MockController.calls(pid)
    end

    test "returns {:ok, crash_info} when crash occurred", %{pid: pid} do
      crash_info = %{exit_status: 11, signal: 11, timestamp: ~U[2026-01-01 00:00:00Z]}
      {:ok, pid} = MockController.start_link(responses: %{verify_crash: {:ok, crash_info}})
      on_exit(fn -> if Process.alive?(pid), do: GenServer.stop(pid) end)

      assert {:ok, ^crash_info} = Deployment.verify_crash(deployment(pid), "s1")
    end

    test "returns {:error, :not_crashed} when no crash", %{pid: pid} do
      {:ok, pid} = MockController.start_link(responses: %{verify_crash: {:error, :not_crashed}})
      on_exit(fn -> if Process.alive?(pid), do: GenServer.stop(pid) end)

      assert {:error, :not_crashed} = Deployment.verify_crash(deployment(pid), "s1")
    end

    test "returns {:error, :timeout} when verify times out", %{pid: pid} do
      {:ok, pid} = MockController.start_link(responses: %{verify_crash: {:error, :timeout}})
      on_exit(fn -> if Process.alive?(pid), do: GenServer.stop(pid) end)

      assert {:error, :timeout} = Deployment.verify_crash(deployment(pid), "s1")
    end
  end

  describe "dead controller" do
    test "expect_crash returns {:error, :controller_not_available}" do
      dead = spawn(fn -> :ok end)
      Process.sleep(50)
      assert {:error, :controller_not_available} = Deployment.expect_crash(deployment(dead), "s1")
    end

    test "verify_crash returns {:error, :controller_not_available}" do
      dead = spawn(fn -> :ok end)
      Process.sleep(50)
      assert {:error, :controller_not_available} = Deployment.verify_crash(deployment(dead), "s1")
    end
  end

  describe "mode independence" do
    setup do
      {:ok, pid} = MockController.start_link()
      on_exit(fn -> if Process.alive?(pid), do: GenServer.stop(pid) end)
      %{pid: pid}
    end

    test "expect_crash works for single_server mode", %{pid: pid} do
      assert :ok = Deployment.expect_crash(deployment(pid, :single_server), "s1")
      assert [{:expect_crash, "s1", 30_000}] = MockController.calls(pid)
    end

    test "expect_crash works for cluster mode", %{pid: pid} do
      assert :ok = Deployment.expect_crash(deployment(pid, :cluster), "dbserver-0")
      assert [{:expect_crash, "dbserver-0", 30_000}] = MockController.calls(pid)
    end

    test "verify_crash works for cluster mode", %{pid: pid} do
      assert {:ok, _} = Deployment.verify_crash(deployment(pid, :cluster), "dbserver-0")
      assert [{:verify_crash, "dbserver-0", 5_000}] = MockController.calls(pid)
    end
  end
end
