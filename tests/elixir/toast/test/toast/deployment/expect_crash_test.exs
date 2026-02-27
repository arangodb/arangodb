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
    Map.get(
      responses,
      :verify_crash,
      {:ok, %{exit_status: 11, signal: 11, timestamp: ~U[2026-01-01 00:00:00Z]}}
    )
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
      {:ok, pid} =
        MockController.start_link(responses: %{expect_crash: {:error, :already_expected}})

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

  describe "expect_crash timeout auto-clear" do
    test "expectation auto-clears after timeout" do
      config = Toast.Config.load()
      {:ok, ctrl} = Toast.Deployment.SingleServerController.start_link(config: config)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      # Set status to :ready so suspend_health_monitor is valid
      :sys.replace_state(ctrl, fn state -> %{state | status: :ready} end)

      # Expect crash with a very short timeout
      :ok = GenServer.call(ctrl, {:expect_crash, state_server_id(ctrl), 50})

      # Verify expectation exists
      state = :sys.get_state(ctrl)
      assert map_size(state.expected_crashes) == 1

      # Wait for the timeout to fire
      Process.sleep(100)

      # Expectation should have been auto-cleared
      state = :sys.get_state(ctrl)
      assert state.expected_crashes == %{}
    end

    test "verify_crash returns {:error, :timeout} when no crash occurs within verify timeout" do
      config = Toast.Config.load()
      {:ok, ctrl} = Toast.Deployment.SingleServerController.start_link(config: config)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      :sys.replace_state(ctrl, fn state -> %{state | status: :ready} end)

      server_id = state_server_id(ctrl)

      # Expect crash with long timeout so the expect_crash_timeout doesn't fire first
      :ok = GenServer.call(ctrl, {:expect_crash, server_id, 10_000})

      # verify_crash with short timeout -- no crash will happen
      result = GenServer.call(ctrl, {:verify_crash, server_id, 200}, 5_000)
      assert result == {:error, :timeout}

      # After verify timeout, expectation is cleaned up
      state = :sys.get_state(ctrl)
      assert state.expected_crashes == %{}
    end

    test "verify_crash returns {:error, :no_expectation} after expect timeout clears" do
      config = Toast.Config.load()
      {:ok, ctrl} = Toast.Deployment.SingleServerController.start_link(config: config)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      :sys.replace_state(ctrl, fn state -> %{state | status: :ready} end)

      server_id = state_server_id(ctrl)

      # Expect crash with very short timeout
      :ok = GenServer.call(ctrl, {:expect_crash, server_id, 50})

      # Wait for auto-clear
      Process.sleep(100)

      # Now verify_crash should fail because there is no expectation
      result = GenServer.call(ctrl, {:verify_crash, server_id, 100}, 5_000)
      assert result == {:error, :no_expectation}
    end

    test "crash during expect window is captured and verify_crash succeeds" do
      config = Toast.Config.load()
      {:ok, ctrl} = Toast.Deployment.SingleServerController.start_link(config: config)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      :sys.replace_state(ctrl, fn state -> %{state | status: :ready} end)

      server_id = state_server_id(ctrl)
      :ok = GenServer.call(ctrl, {:expect_crash, server_id, 5_000})

      # Simulate crash notification
      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(ctrl, {:server_crashed, server_id, crash_info})

      # Give handle_info time to process
      Process.sleep(50)

      # verify_crash should return the crash info
      assert {:ok, returned_info} = GenServer.call(ctrl, {:verify_crash, server_id, 1_000}, 5_000)
      assert returned_info.signal == 11
      assert returned_info.exit_status == 139
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

  defp state_server_id(ctrl) do
    state = :sys.get_state(ctrl)
    state.server.id
  end
end
