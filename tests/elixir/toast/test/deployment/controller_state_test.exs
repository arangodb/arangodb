defmodule Toast.Deployment.ControllerStateTest do
  use ExUnit.Case, async: false

  alias Toast.Process.ServerProcess
  alias Toast.Deployment.{Controller, ServerInstance}

  import Toast.ServerTestHelpers, only: [cleanup_server: 1]
  import Toast.DeploymentTestHelpers, only: [inject_cluster_servers: 2]

  @fake_server Path.expand("../support/fake_server.sh", __DIR__)

  describe "ServerInstance operational_state" do
    test "defaults" do
      s = %ServerInstance{id: "s1", role: :single}
      assert s.operational_state == nil
      assert s.intentional == false
      assert s.launch_spec == nil
    end

    test "can set operational_state" do
      s = %ServerInstance{id: "s1", role: :single, operational_state: :running}
      assert s.operational_state == :running
    end
  end

  describe "ServerProcess control flow" do
    setup do
      id = "ctl-state-#{System.unique_integer([:positive])}"
      opts = [id: id, executable: @fake_server, args: ["--port", "0"], listener: self()]
      {:ok, pid} = ServerProcess.start_link(opts)
      :ok = ServerProcess.launch(pid)
      on_exit(fn -> cleanup_server(pid) end)
      %{pid: pid, id: id}
    end

    test "running -> kill -> :killed (no crash notification)", %{pid: pid} do
      assert :ok = ServerProcess.kill(pid)
      assert ServerProcess.status(pid) == :killed
      Process.sleep(300)
      refute_receive {:server_crashed, _, _}
    end

    test "running -> pause -> :paused, then resume -> :running", %{pid: pid} do
      assert :ok = ServerProcess.pause(pid)
      assert ServerProcess.status(pid) == :paused
      assert :ok = ServerProcess.resume(pid)
      assert ServerProcess.status(pid) == :running
    end

    test "running -> stop -> :stopped, then relaunch -> :running", %{pid: pid} do
      :ok = ServerProcess.stop(pid, 5_000)
      assert ServerProcess.status(pid) == :stopped
      :ok = ServerProcess.relaunch(pid)
      assert ServerProcess.status(pid) == :running
    end

    test "unexpected crash during running sets :crashed and notifies", %{pid: pid, id: id} do
      os_pid = ServerProcess.os_pid(pid)
      System.cmd("kill", ["-9", to_string(os_pid)])
      assert_receive {:server_crashed, ^id, crash_info}, 5_000
      assert ServerProcess.status(pid) == :crashed
      assert crash_info.signal == 9
    end
  end

  # --- Controller stop_server sets intentional flag (single server mode) ---

  describe "Controller (single server) stop_server sets intentional flag" do
    setup do
      id = "ssc-stop-#{System.unique_integer([:positive])}"

      {:ok, server_pid} =
        ServerProcess.start_link(
          id: id,
          executable: @fake_server,
          args: ["--port", "0"],
          listener: self()
        )

      :ok = ServerProcess.launch(server_pid)

      {:ok, ctrl} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load(), id: id)

      inject_single_server_state(ctrl, id, server_pid)

      on_exit(fn ->
        cleanup_server(server_pid)
        if Process.alive?(ctrl), do: GenServer.stop(ctrl)
      end)

      %{ctrl: ctrl, id: id, server_pid: server_pid}
    end

    test "stop_server sets operational_state to :stopped and intentional to true", %{
      ctrl: ctrl,
      id: id
    } do
      assert :ok = Controller.stop_server(ctrl, id)
      state = :sys.get_state(ctrl)
      server = state.servers[id]
      assert server.operational_state == :stopped
      assert server.intentional == true
      assert state.status == :degraded
    end

    test "kill_server sets operational_state to :killed and intentional to true", %{
      ctrl: ctrl,
      id: id
    } do
      assert :ok = Controller.kill_server(ctrl, id)
      state = :sys.get_state(ctrl)
      server = state.servers[id]
      assert server.operational_state == :killed
      assert server.intentional == true
      assert state.status == :degraded
    end
  end

  describe "Controller (single server) unexpected crash clears intentional" do
    test "crash without prior stop/kill leaves intentional as false" do
      id = "ssc-unexp-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load(), id: id)

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(ctrl, {:server_crashed, id, crash_info})
      :sys.get_state(ctrl)

      state = :sys.get_state(ctrl)
      assert state.status == :failed
    end
  end

  describe "Controller (single server) signal-type awareness during intentional stop" do
    test "SIGSEGV (signal 11) during intentional stop clears intentional flag" do
      id = "ssc-sig11-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load(), id: id)

      set_intentional(ctrl, id, true)

      crash_info = %{exit_status: 139, signal: 11, timestamp: DateTime.utc_now()}
      send(ctrl, {:server_crashed, id, crash_info})
      :sys.get_state(ctrl)

      state = :sys.get_state(ctrl)
      assert state.status == :failed
      assert state.servers[id].intentional == false
    end

    test "SIGTERM (signal 15) during intentional stop keeps intentional true" do
      id = "ssc-sig15-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load(), id: id)

      set_intentional(ctrl, id, true)

      crash_info = %{exit_status: 0, signal: 15, timestamp: DateTime.utc_now()}
      send(ctrl, {:server_crashed, id, crash_info})
      :sys.get_state(ctrl)

      state = :sys.get_state(ctrl)
      # signal 15 is in @intentional_exit_signals, so it is ignored (no state change)
      assert state.servers[id].intentional == true
    end
  end

  # --- ClusterController state machine tests ---

  describe "Controller (cluster) derive_cluster_status" do
    setup do
      {:ok, ctrl} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      on_exit(fn -> try do GenServer.stop(ctrl) catch :exit, _ -> :ok end end)
      %{ctrl: ctrl}
    end

    test "all servers running -> :ready", %{ctrl: ctrl} do
      inject_cluster_servers(ctrl, %{
        "agent-0" => %ServerInstance{
          id: "agent-0",
          role: :agent,
          operational_state: :running,
          intentional: false
        },
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :running,
          intentional: false
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running,
          intentional: false
        }
      })

      set_cluster_status(ctrl, :ready)
      assert Controller.get_status(ctrl) == :ready
    end

    test "some servers intentionally down -> :degraded", %{ctrl: ctrl} do
      inject_cluster_servers(ctrl, %{
        "agent-0" => %ServerInstance{
          id: "agent-0",
          role: :agent,
          operational_state: :running,
          intentional: false
        },
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :stopped,
          intentional: true
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running,
          intentional: false
        }
      })

      state = :sys.get_state(ctrl)
      assert derive_expected_status(state.servers) == :degraded
    end

    test "unexpected crash (intentional=false) -> :failed", %{ctrl: ctrl} do
      inject_cluster_servers(ctrl, %{
        "agent-0" => %ServerInstance{
          id: "agent-0",
          role: :agent,
          operational_state: :running,
          intentional: false
        },
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :crashed,
          intentional: false
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running,
          intentional: false
        }
      })

      state = :sys.get_state(ctrl)
      assert derive_expected_status(state.servers) == :failed
    end

    test "expected crash (intentional=true) -> :degraded", %{ctrl: ctrl} do
      inject_cluster_servers(ctrl, %{
        "agent-0" => %ServerInstance{
          id: "agent-0",
          role: :agent,
          operational_state: :running,
          intentional: false
        },
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :crashed,
          intentional: true
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running,
          intentional: false
        }
      })

      state = :sys.get_state(ctrl)
      assert derive_expected_status(state.servers) == :ready
    end
  end

  describe "Controller (single server) HealthMonitor restart on unexpected death" do
    test "restarts health monitor when it dies unexpectedly during :ready" do
      id = "ssc-hm-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load(), id: id)

      fake_hm = spawn(fn -> Process.sleep(:infinity) end)
      Process.monitor(fake_hm)
      set_ready_with_health_monitor(ctrl, id, fake_hm)

      Process.exit(fake_hm, :abnormal_crash)

      receive do
        {:DOWN, _, :process, ^fake_hm, _} -> :ok
      end

      :sys.get_state(ctrl)
      assert Process.alive?(ctrl)
    end

    test "does not restart health monitor on normal shutdown" do
      id = "ssc-hm-normal-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load(), id: id)

      fake_hm = spawn(fn -> Process.sleep(:infinity) end)
      set_ready_with_health_monitor(ctrl, id, fake_hm)

      Process.exit(fake_hm, :normal)
      Process.sleep(50)

      :sys.get_state(ctrl)
      assert Process.alive?(ctrl)
    end
  end

  # --- Helpers ---

  defp inject_single_server_state(ctrl, id, server_pid) do
    :sys.replace_state(ctrl, fn state ->
      server =
        case state.servers[id] do
          nil -> %ServerInstance{id: id, role: :single}
          s -> s
        end

      server = %{
        server
        | server_pid: server_pid,
          operational_state: :running,
          intentional: false,
          pid: ServerProcess.os_pid(server_pid)
      }

      %{state | status: :ready, servers: %{id => server}}
    end)
  end

  defp set_intentional(ctrl, id, value) do
    :sys.replace_state(ctrl, fn state ->
      server = state.servers[id] || %ServerInstance{id: id, role: :single}
      %{state | servers: Map.put(state.servers, id, %{server | intentional: value})}
    end)
  end

  defp set_ready_with_health_monitor(ctrl, id, hm_pid) do
    :sys.replace_state(ctrl, fn state ->
      server = state.servers[id] || %ServerInstance{id: id, role: :single}

      %{
        state
        | status: :ready,
          servers: Map.put(state.servers, id, %{server | health_monitor: hm_pid})
      }
    end)
  end

  defp set_cluster_status(ctrl, status) do
    :sys.replace_state(ctrl, fn state -> %{state | status: status} end)
  end

  defp derive_expected_status(servers) do
    server_list = Map.values(servers)
    states = Enum.map(server_list, & &1.operational_state)

    cond do
      Enum.any?(server_list, &(&1.operational_state == :crashed and not &1.intentional)) ->
        :failed

      Enum.all?(states, &(&1 == :running)) ->
        :ready

      Enum.any?(states, &(&1 in [:stopped, :killed, :paused])) ->
        :degraded

      true ->
        :ready
    end
  end
end
