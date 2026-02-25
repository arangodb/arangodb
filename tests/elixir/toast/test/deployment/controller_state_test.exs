defmodule Toast.Deployment.ControllerStateTest do
  use ExUnit.Case, async: false

  alias Toast.Process.ServerProcess
  alias Toast.Deployment.ServerInstance

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

    defp cleanup_server(pid) do
      if Process.alive?(pid) do
        try do
          ServerProcess.stop(pid, 2_000)
        catch
          :exit, _ -> :ok
        end
      end

      if Process.alive?(pid) do
        ref = Process.monitor(pid)
        try do GenServer.stop(pid, :normal, 1_000) catch :exit, _ -> :ok end
        receive do {:DOWN, ^ref, _, _, _} -> :ok after 1_000 -> :ok end
      end
    end
  end
end
