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
      {:ok,
       %Toast.Process.CrashInfo{
         exit_status: 11,
         signal: 11,
         timestamp: DateTime.to_unix(~U[2026-01-01 00:00:00Z], :microsecond)
       }}
    )
  end

  defp lookup_response(_responses, _msg), do: :ok
end

defmodule Toast.Deployment.ExpectCrashTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.Config
  alias Toast.Deployment
  alias Toast.Deployment.{Controller, ServerInstance}
  alias Toast.Deployment.ExpectCrashTest.MockController
  alias Toast.Process.CrashInfo

  defp deployment(pid) do
    %Deployment{
      id: "test",
      controller: pid
    }
  end

  describe "expect_crash delegation" do
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

    test "sends {:expect_crash, server_id, timeout} with default 30_000", %{pid: pid} do
      assert :ok = Deployment.expect_crash(deployment(pid), "s1")
      assert [{:expect_crash, "s1", 30_000}] = MockController.calls(pid)
    end

    test "custom timeout passes through", %{pid: pid} do
      assert :ok = Deployment.expect_crash(deployment(pid), "s1", timeout: 60_000)
      assert [{:expect_crash, "s1", 60_000}] = MockController.calls(pid)
    end

    test "returns {:error, :already_expected} from controller", _context do
      {:ok, pid} =
        MockController.start_link(responses: %{expect_crash: {:error, :already_expected}})

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :already_expected} = Deployment.expect_crash(deployment(pid), "s1")
    end
  end

  describe "verify_crash delegation" do
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

    test "sends {:verify_crash, server_id, timeout} with default 5_000", %{pid: pid} do
      assert {:ok, _crash_info} = Deployment.verify_crash(deployment(pid), "s1")
      assert [{:verify_crash, "s1", 5_000}] = MockController.calls(pid)
    end

    test "custom timeout passes through", %{pid: pid} do
      Deployment.verify_crash(deployment(pid), "s1", timeout: 15_000)
      assert [{:verify_crash, "s1", 15_000}] = MockController.calls(pid)
    end

    test "returns {:ok, crash_info} when crash occurred", _context do
      crash_info = %Toast.Process.CrashInfo{
        exit_status: 11,
        signal: 11,
        timestamp: DateTime.to_unix(~U[2026-01-01 00:00:00Z], :microsecond)
      }

      {:ok, pid} = MockController.start_link(responses: %{verify_crash: {:ok, crash_info}})

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:ok, ^crash_info} = Deployment.verify_crash(deployment(pid), "s1")
    end

    test "returns {:error, :not_crashed} when no crash", _context do
      {:ok, pid} = MockController.start_link(responses: %{verify_crash: {:error, :not_crashed}})

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

      assert {:error, :not_crashed} = Deployment.verify_crash(deployment(pid), "s1")
    end

    test "returns {:error, :timeout} when verify times out", _context do
      {:ok, pid} = MockController.start_link(responses: %{verify_crash: {:error, :timeout}})

      on_exit(fn ->
        try do
          GenServer.stop(pid)
        catch
          :exit, _ -> :ok
        end
      end)

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
    setup do
      id = "expect-crash-#{System.unique_integer([:positive])}"
      server = %ServerInstance{id: id, role: :single}

      {:ok, ctrl} =
        Controller.start_link(
          config: Config.new(),
          id: id,
          servers: %{id => server},
          status: :ready
        )

      on_exit(fn ->
        try do
          GenServer.stop(ctrl)
        catch
          :exit, _ -> :ok
        end
      end)

      %{ctrl: ctrl, server_id: id}
    end

    test "expectation auto-clears after timeout", %{ctrl: ctrl, server_id: server_id} do
      :ok = Controller.expect_crash(ctrl, server_id, 50)

      # Wait for the timeout to fire
      Process.sleep(100)

      # Verify expectation was auto-cleared
      result = Controller.verify_crash(ctrl, server_id, 100)
      assert result == {:error, :no_expectation}
    end

    test "verify_crash returns {:error, :timeout} when no crash occurs within verify timeout",
         %{ctrl: ctrl, server_id: server_id} do
      :ok = Controller.expect_crash(ctrl, server_id, 10_000)

      # verify_crash with short timeout -- no crash will happen
      result = Controller.verify_crash(ctrl, server_id, 200)
      assert result == {:error, :timeout}

      # After verify timeout, expectation is cleaned up
      result = Controller.verify_crash(ctrl, server_id, 100)
      assert result == {:error, :no_expectation}
    end

    test "crash during expect window is captured and verify_crash succeeds",
         %{ctrl: ctrl, server_id: server_id} do
      :ok = Controller.expect_crash(ctrl, server_id, 5_000)

      info = %CrashInfo{
        exit_status: 139,
        signal: 11,
        timestamp: :os.system_time(:microsecond)
      }

      Controller.notify_crash(ctrl, server_id, info)

      assert {:ok, returned_info} = Controller.verify_crash(ctrl, server_id, 1_000)
      assert returned_info.signal == 11
      assert returned_info.exit_status == 139
    end
  end

  describe "expect_crash with different server IDs" do
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

    test "expect_crash works for any server ID", %{pid: pid} do
      assert :ok = Deployment.expect_crash(deployment(pid), "dbserver-0")
      assert [{:expect_crash, "dbserver-0", 30_000}] = MockController.calls(pid)
    end

    test "verify_crash works for any server ID", %{pid: pid} do
      assert {:ok, _} = Deployment.verify_crash(deployment(pid), "dbserver-0")
      assert [{:verify_crash, "dbserver-0", 5_000}] = MockController.calls(pid)
    end
  end
end
