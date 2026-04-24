defmodule Toast.Deployment.ControllerStateTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.{Controller, ServerInstance}
  alias Toast.Process.CrashInfo

  @moduletag :unit

  # --- ServerInstance struct defaults ---

  describe "ServerInstance operational_state" do
    test "defaults" do
      s = %ServerInstance{id: "s1", role: :single}
      assert s.operational_state == nil
      assert s.expecting_exit == false
      assert s.launch_spec == nil
    end

    test "can set operational_state" do
      s = %ServerInstance{id: "s1", role: :single, operational_state: :running}
      assert s.operational_state == :running
    end
  end

  # --- Controller stop/kill sets expecting_exit flag ---

  describe "Controller (single server) stop_server sets expecting_exit flag" do
    setup do
      id = "ssc-stop-#{System.unique_integer([:positive])}"

      {:ok, server_pid} =
        Toast.Process.ServerProcess.start_link(
          id: id,
          executable: fake_server(),
          args: ["--port", "0"],
          listener: self()
        )

      :ok = Toast.Process.ServerProcess.launch(server_pid)

      # Injected :ready status without a real deploy -- the test verifies
      # Controller state transitions, not the deploy pipeline.
      server = %ServerInstance{
        id: id,
        role: :single,
        server_pid: server_pid,
        operational_state: :running,
        expecting_exit: false,
        pid: Toast.Process.ServerProcess.os_pid(server_pid)
      }

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => server},
          status: :ready
        )

      on_exit(fn ->
        cleanup_server(server_pid)
        if Process.alive?(ctrl), do: GenServer.stop(ctrl)
      end)

      %{ctrl: ctrl, id: id, server_pid: server_pid}
    end

    test "stop_server sets operational_state to :stopped and expecting_exit to true", %{
      ctrl: ctrl,
      id: id
    } do
      assert :ok = Controller.stop_server(ctrl, id)
      info = Controller.get_info(ctrl)
      server = info.servers[id]
      assert server.operational_state == :stopped
      assert server.expecting_exit == true
      assert info.status == :degraded
    end

    test "kill_server sets operational_state to :killed and expecting_exit to true", %{
      ctrl: ctrl,
      id: id
    } do
      assert :ok = Controller.kill_server(ctrl, id)
      info = Controller.get_info(ctrl)
      server = info.servers[id]
      assert server.operational_state == :killed
      assert server.expecting_exit == true
      assert info.status == :degraded
    end
  end

  # --- Unexpected crash handling ---

  describe "Controller (single server) unexpected crash" do
    test "crash without prior stop/kill transitions to :failed" do
      id = "ssc-unexp-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => %ServerInstance{id: id, role: :single}}
        )

      Controller.notify_crash(ctrl, id, make_crash_info(signal: 11, exit_status: 139))

      info = Controller.get_info(ctrl)
      assert info.status == :failed
    end
  end

  # --- Signal-type awareness during expected exit ---

  describe "Controller (single server) signal-type awareness during expected exit" do
    test "SIGSEGV (signal 11) during expected exit clears expecting_exit and fails" do
      id = "ssc-sig11-#{System.unique_integer([:positive])}"

      # Injected expecting_exit: true without a real stop -- tests the crash
      # classification logic in isolation.
      server = %ServerInstance{id: id, role: :single, expecting_exit: true}

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => server}
        )

      Controller.notify_crash(ctrl, id, make_crash_info(signal: 11, exit_status: 139))

      info = Controller.get_info(ctrl)
      assert info.status == :failed
      assert info.servers[id].expecting_exit == false
    end

    test "SIGKILL (signal 9) during expected exit clears expecting_exit and fails" do
      id = "ssc-sig9-#{System.unique_integer([:positive])}"

      server = %ServerInstance{id: id, role: :single, expecting_exit: true}

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => server}
        )

      Controller.notify_crash(ctrl, id, make_crash_info(signal: 9, exit_status: 137))

      info = Controller.get_info(ctrl)
      assert info.status == :failed
      assert info.servers[id].expecting_exit == false
    end

    test "SIGTERM (signal 15) during expected exit is silently handled" do
      id = "ssc-sig15-#{System.unique_integer([:positive])}"

      # Injected expecting_exit: true -- tests intentional exit classification.
      server = %ServerInstance{id: id, role: :single, expecting_exit: true}

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => server}
        )

      Controller.notify_crash(ctrl, id, make_crash_info(signal: 15, exit_status: 0))

      info = Controller.get_info(ctrl)
      # signal 15 is in @intentional_exit_signals -- no state change
      assert info.servers[id].expecting_exit == true
    end

    test "clean shutdown (signal nil, exit_status 0) during expected exit is silently handled" do
      id = "ssc-clean-#{System.unique_integer([:positive])}"

      server = %ServerInstance{id: id, role: :single, expecting_exit: true}

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => server}
        )

      Controller.notify_crash(ctrl, id, make_crash_info(signal: nil, exit_status: 0))

      info = Controller.get_info(ctrl)
      # signal nil is in @intentional_exit_signals -- no state change
      assert info.servers[id].expecting_exit == true
    end
  end

  # --- Unknown server_id crash handling ---

  describe "Controller crash notification for unknown server_id" do
    test "unknown server_id is ignored without crashing the Controller" do
      id = "ssc-unknown-#{System.unique_integer([:positive])}"

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => %ServerInstance{id: id, role: :single, operational_state: :running}},
          status: :ready
        )

      Controller.notify_crash(ctrl, "nonexistent-server", make_crash_info())

      # Controller should still be alive and state unchanged
      assert Controller.get_status(ctrl) == :ready
    end
  end

  # --- server_unhealthy handler ---

  describe "Controller server_unhealthy handler" do
    test "transitions to :failed and sets error" do
      id = "ssc-unhealthy-#{System.unique_integer([:positive])}"

      # No real server_pid needed -- we just verify the state transition.
      # The send_signal call is guarded by `if server && server.server_pid`.
      server = %ServerInstance{id: id, role: :single, operational_state: :running}

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => server},
          status: :ready
        )

      send(ctrl, {:server_unhealthy, id})

      info = Controller.get_info(ctrl)
      assert info.status == :failed
      assert info.error == {:server_unhealthy, id}
      assert info.servers[id].operational_state == :killed
      assert info.servers[id].expecting_exit == true
    end
  end

  # --- Cluster status derivation ---

  describe "Controller (cluster) derive_cluster_status" do
    test "all servers running -> :ready" do
      servers = %{
        "agent-0" => %ServerInstance{id: "agent-0", role: :agent, operational_state: :running},
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :running
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running
        }
      }

      assert ServerInstance.derive_cluster_status(servers) == :ready
    end

    test "some servers intentionally stopped -> :degraded" do
      servers = %{
        "agent-0" => %ServerInstance{id: "agent-0", role: :agent, operational_state: :running},
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :stopped,
          expecting_exit: true
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running
        }
      }

      assert ServerInstance.derive_cluster_status(servers) == :degraded
    end

    test "unexpected crash -> :failed" do
      servers = %{
        "agent-0" => %ServerInstance{id: "agent-0", role: :agent, operational_state: :running},
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :crashed,
          expecting_exit: false
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running
        }
      }

      assert ServerInstance.derive_cluster_status(servers) == :failed
    end

    test "expected crash -> :degraded (not :failed)" do
      servers = %{
        "agent-0" => %ServerInstance{id: "agent-0", role: :agent, operational_state: :running},
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :crashed,
          expecting_exit: true
        },
        "coordinator-0" => %ServerInstance{
          id: "coordinator-0",
          role: :coordinator,
          operational_state: :running
        }
      }

      assert ServerInstance.derive_cluster_status(servers) == :degraded
    end

    test "empty servers map -> :ready" do
      assert ServerInstance.derive_cluster_status(%{}) == :ready
    end

    test "all servers crashed unexpectedly -> :failed" do
      servers = %{
        "agent-0" => %ServerInstance{
          id: "agent-0",
          role: :agent,
          operational_state: :crashed,
          expecting_exit: false
        },
        "dbserver-0" => %ServerInstance{
          id: "dbserver-0",
          role: :dbserver,
          operational_state: :crashed,
          expecting_exit: false
        }
      }

      assert ServerInstance.derive_cluster_status(servers) == :failed
    end
  end

  # --- HealthMonitor restart on unexpected death ---

  describe "Controller (single server) HealthMonitor restart on unexpected death" do
    test "restarts health monitor when it dies unexpectedly during :ready" do
      id = "ssc-hm-#{System.unique_integer([:positive])}"

      fake_hm = spawn(fn -> Process.sleep(:infinity) end)

      # Injected :ready with a fake health_monitor pid -- tests the DOWN
      # handler's restart logic without a real deployment or HTTP endpoint.
      # The restart calls ProcessSupervisor.start_health_monitor which
      # requires Toast.Process.Supervisor to be running (started by the app).
      server = %ServerInstance{
        id: id,
        role: :single,
        operational_state: :running,
        health_monitor: fake_hm,
        endpoint: "http://localhost:0"
      }

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => server},
          status: :ready
        )

      Process.monitor(fake_hm)
      Process.exit(fake_hm, :abnormal_crash)

      receive do
        {:DOWN, _, :process, ^fake_hm, _} -> :ok
      end

      # Synchronous call forces the Controller to process the :DOWN message first
      info = Controller.get_info(ctrl)
      new_hm = info.servers[id].health_monitor
      assert new_hm != fake_hm, "expected a new health monitor pid, got the old one"
      assert is_pid(new_hm)
      assert Process.alive?(new_hm)
    end

    test "does not restart health monitor on normal shutdown" do
      id = "ssc-hm-normal-#{System.unique_integer([:positive])}"

      fake_hm = spawn(fn -> Process.sleep(:infinity) end)

      server = %ServerInstance{
        id: id,
        role: :single,
        operational_state: :running,
        health_monitor: fake_hm
      }

      {:ok, ctrl} =
        Controller.start_link(
          config: Toast.Deployment.Config.new(),
          id: id,
          servers: %{id => server},
          status: :ready
        )

      Process.exit(fake_hm, :normal)

      # Synchronous call forces message ordering
      info = Controller.get_info(ctrl)
      # Normal exit should not trigger restart -- health_monitor stays as old pid
      assert info.servers[id].health_monitor == fake_hm
    end
  end

  # --- Helpers ---

  defp fake_server, do: Path.expand("../support/fake_server.sh", __DIR__)

  defp make_crash_info(opts \\ []) do
    %CrashInfo{
      exit_status: Keyword.get(opts, :exit_status, 139),
      signal: Keyword.get(opts, :signal, 11),
      timestamp: :os.system_time(:microsecond)
    }
  end

  defp cleanup_server(pid) do
    Toast.Process.ServerProcess.shutdown(pid, 2_000)
  end
end
