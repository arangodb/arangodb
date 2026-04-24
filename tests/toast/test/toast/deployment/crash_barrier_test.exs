defmodule Toast.Deployment.CrashBarrierTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment
  alias Toast.Deployment.{Config, Controller, CrashBarrier, ServerInstance}
  alias Toast.Process.CrashInfo

  defp start_deployment(servers, status \\ :ready) do
    id = "barrier-test-#{System.unique_integer([:positive])}"
    server_map = Map.new(servers, fn server -> {server.id, server} end)

    {:ok, ctrl} =
      Controller.start_link(
        config: Config.new(),
        id: id,
        servers: server_map,
        status: status
      )

    on_exit(fn ->
      try do
        GenServer.stop(ctrl)
      catch
        :exit, _ -> :ok
      end
    end)

    %Deployment{id: id, controller: ctrl}
  end

  defp server(id, opts \\ []) do
    defaults = [
      id: id,
      role: :single,
      operational_state: :running,
      pid: 12_345,
      expecting_exit: false
    ]

    struct!(ServerInstance, Keyword.merge(defaults, opts))
  end

  defp crash_info do
    %CrashInfo{exit_status: 139, signal: 11, timestamp: :os.system_time(:microsecond)}
  end

  describe "await_settled/2" do
    test "returns :ok immediately when all servers are alive" do
      deployment = start_deployment([server("a"), server("b")])
      inspector = fn _pid -> :alive end

      assert CrashBarrier.await_settled(deployment,
               inspector: inspector,
               timeout: 1_000
             ) == :ok
    end

    test "returns :ok without probing when controller is already :failed" do
      deployment = start_deployment([server("a")], :failed)

      inspector = fn _pid -> flunk("inspector should not be called when already :failed") end

      assert CrashBarrier.await_settled(deployment,
               inspector: inspector,
               timeout: 1_000
             ) == :ok
    end

    test "blocks and returns :ok when a crashing server's event arrives" do
      deployment = start_deployment([server("a"), server("b")])

      inspector = fn
        12_345 -> {:crashing, :core_dumping}
        _ -> :alive
      end

      task =
        Task.async(fn ->
          CrashBarrier.await_settled(deployment, inspector: inspector, timeout: 5_000)
        end)

      Process.sleep(50)
      refute Task.yield(task, 0)

      Controller.notify_crash(deployment.controller, "a", crash_info())

      assert Task.await(task, 1_000) == :ok
    end

    test "returns {:error, message} when crash event never arrives" do
      deployment = start_deployment([server("a")])
      inspector = fn _pid -> {:crashing, :core_dumping} end

      assert {:error, message} =
               CrashBarrier.await_settled(deployment, inspector: inspector, timeout: 100)

      assert message =~ "Server a"
      assert message =~ "reason=core_dumping"
      assert message =~ "100ms"
    end

    test "skips servers whose operational_state is not :running" do
      deployment =
        start_deployment([
          server("a", operational_state: :stopped),
          server("b", operational_state: :crashed),
          server("c", operational_state: :paused)
        ])

      inspector = fn _pid -> flunk("inspector should not be called for non-running servers") end

      assert CrashBarrier.await_settled(deployment, inspector: inspector, timeout: 1_000) == :ok
    end

    test "skips servers with nil pid" do
      deployment = start_deployment([server("a", pid: nil)])
      inspector = fn _pid -> flunk("inspector should not be called when pid is nil") end

      assert CrashBarrier.await_settled(deployment, inspector: inspector, timeout: 1_000) == :ok
    end

    test "detects crash on second server when first is alive" do
      deployment = start_deployment([server("a", pid: 111), server("b", pid: 222)])

      inspector = fn
        111 -> :alive
        222 -> {:crashing, :zombie}
      end

      task =
        Task.async(fn ->
          CrashBarrier.await_settled(deployment, inspector: inspector, timeout: 2_000)
        end)

      Process.sleep(50)
      Controller.notify_crash(deployment.controller, "b", crash_info())

      assert Task.await(task, 1_000) == :ok
    end

    test "preserves the detected reason in the error message" do
      deployment = start_deployment([server("a")])
      inspector = fn _pid -> {:crashing, :proc_missing} end

      assert {:error, message} =
               CrashBarrier.await_settled(deployment, inspector: inspector, timeout: 100)

      assert message =~ "reason=proc_missing"
    end
  end
end
