defmodule Toast.Process.ServerProcessTest do
  use ExUnit.Case, async: false

  alias Toast.Process.ServerProcess
  alias Toast.Process.Signal

  @fake_server Path.expand("../support/fake_server.sh", __DIR__)

  setup do
    id = "test-server-#{System.unique_integer([:positive])}"
    %{id: id}
  end

  defp start_server(id, extra_opts \\ []) do
    opts =
      Keyword.merge(
        [id: id, executable: @fake_server, args: ["--port", "0"]],
        extra_opts
      )

    {:ok, pid} = ServerProcess.start_link(opts)

    on_exit(fn ->
      if Process.alive?(pid) do
        try do
          ServerProcess.stop(pid, 2_000)
        catch
          :exit, _ -> :ok
        end

        ref = Process.monitor(pid)
        GenServer.stop(pid, :normal, 1_000)

        receive do
          {:DOWN, ^ref, _, _, _} -> :ok
        after
          1_000 -> :ok
        end
      end
    end)

    pid
  end

  describe "launch and stop" do
    test "starts, launches, and stops gracefully", %{id: id} do
      pid = start_server(id)

      assert ServerProcess.status(pid) == :stopped
      assert ServerProcess.launch(pid) == :ok
      assert ServerProcess.status(pid) == :running
      assert ServerProcess.stop(pid, 5_000) == :ok
      assert ServerProcess.status(pid) == :stopped
    end
  end

  describe "os_pid/1" do
    test "returns a positive integer when running", %{id: id} do
      pid = start_server(id)
      assert ServerProcess.os_pid(pid) == nil

      ServerProcess.launch(pid)
      os_pid = ServerProcess.os_pid(pid)

      assert is_integer(os_pid)
      assert os_pid > 0
      assert Signal.alive?(os_pid)
    end

    test "returns nil when stopped", %{id: id} do
      pid = start_server(id)
      assert ServerProcess.os_pid(pid) == nil
    end
  end

  describe "id/1" do
    test "returns the server id", %{id: id} do
      pid = start_server(id)
      assert ServerProcess.id(pid) == id
    end
  end

  describe "graceful SIGTERM stop" do
    test "process exits cleanly after SIGTERM", %{id: id} do
      pid = start_server(id)
      ServerProcess.launch(pid)
      os_pid = ServerProcess.os_pid(pid)

      assert Signal.alive?(os_pid)
      assert ServerProcess.stop(pid, 5_000) == :ok
      assert ServerProcess.status(pid) == :stopped

      Process.sleep(100)
      refute Signal.alive?(os_pid)
    end
  end

  describe "crash detection" do
    test "notifies listener on crash", %{id: id} do
      pid = start_server(id, args: ["--crash-after", "1"], listener: self())
      ServerProcess.launch(pid)

      assert_receive {:server_crashed, ^id, crash_info}, 5_000
      assert crash_info.exit_status == 1
      assert %DateTime{} = crash_info.timestamp
      assert ServerProcess.status(pid) == :crashed
    end

    test "crash_info contains signal field", %{id: id} do
      pid = start_server(id, args: ["--crash-after", "1"], listener: self())
      ServerProcess.launch(pid)

      assert_receive {:server_crashed, ^id, crash_info}, 5_000
      assert Map.has_key?(crash_info, :signal)
    end
  end

  describe "stop already stopped" do
    test "returns :ok", %{id: id} do
      pid = start_server(id)
      assert ServerProcess.status(pid) == :stopped
      assert ServerProcess.stop(pid, 5_000) == :ok
    end
  end

  describe "launch already running" do
    test "returns error", %{id: id} do
      pid = start_server(id)
      ServerProcess.launch(pid)

      assert {:error, {:already_launched, :running}} = ServerProcess.launch(pid)
    end
  end

  describe "output capture" do
    test "captures stdout into output_buffer", %{id: id} do
      pid = start_server(id)
      ServerProcess.launch(pid)

      # Give the process time to write its startup message
      Process.sleep(500)

      state = :sys.get_state(pid)
      output = IO.iodata_to_binary(state.output_buffer)
      assert output =~ "STARTED port="

      ServerProcess.stop(pid, 5_000)
      assert ServerProcess.status(pid) == :stopped
    end
  end

  describe "stop after crash" do
    test "returns :ok for a crashed process", %{id: id} do
      pid = start_server(id, args: ["--crash-after", "1"], listener: self())
      ServerProcess.launch(pid)

      assert_receive {:server_crashed, ^id, _crash_info}, 5_000
      assert ServerProcess.status(pid) == :crashed
      assert ServerProcess.stop(pid, 5_000) == :ok
    end
  end

  describe "kill escalation" do
    @unkillable_server Path.expand("../support/unkillable_server.sh", __DIR__)

    test "escalates to SIGKILL when SIGTERM is ignored", %{id: id} do
      pid = start_server(id, executable: @unkillable_server, args: [])
      ServerProcess.launch(pid)
      os_pid = ServerProcess.os_pid(pid)
      assert Signal.alive?(os_pid)

      # Use a short SIGTERM timeout (1s) so kill escalation fires quickly.
      # GenServer.call timeout inside stop/2 is timeout + 5_000 = 6_000ms,
      # which is plenty for SIGTERM wait (1s) + SIGKILL to take effect.
      assert ServerProcess.stop(pid, 1_000) == :ok
      assert ServerProcess.status(pid) == :stopped

      # OS process should be dead after SIGKILL
      Process.sleep(100)
      refute Signal.alive?(os_pid)
    end
  end
end
