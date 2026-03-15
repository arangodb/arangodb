defmodule Toast.Process.ServerProcessTest do
  use ExUnit.Case, async: false

  alias Toast.Process.ServerProcess

  import Toast.ServerTestHelpers, only: [cleanup_server: 1, os_process_alive?: 1]

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
    on_exit(fn -> cleanup_server(pid) end)
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
      assert os_process_alive?(os_pid)
    end

    test "returns nil when stopped", %{id: id} do
      pid = start_server(id)
      assert ServerProcess.os_pid(pid) == nil
    end
  end

  describe "server id" do
    test "crash notification includes the correct server id", %{id: id} do
      pid = start_server(id, args: ["--crash-after", "1"], listener: self())
      ServerProcess.launch(pid)

      assert_receive {:server_crashed, ^id, _crash_info}, 5_000
    end
  end

  describe "graceful SIGTERM stop" do
    test "process exits cleanly after SIGTERM", %{id: id} do
      pid = start_server(id)
      ServerProcess.launch(pid)
      os_pid = ServerProcess.os_pid(pid)

      assert os_process_alive?(os_pid)
      assert ServerProcess.stop(pid, 5_000) == :ok
      assert ServerProcess.status(pid) == :stopped

      Process.sleep(100)
      refute os_process_alive?(os_pid)
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
      assert os_process_alive?(os_pid)

      # Give the bash trap a moment to fully set up
      Process.sleep(200)
      assert os_process_alive?(os_pid), "process died before stop was called"

      # The unkillable server traps SIGTERM but not SIGABRT, so escalation
      # (SIGABRT → SIGKILL) is required.
      assert ServerProcess.stop(pid, 500) == :escalated
      assert ServerProcess.status(pid) == :stopped

      Process.sleep(100)
      refute os_process_alive?(os_pid)
    end
  end

  describe "full escalation chain (SIGTERM → SIGABRT → SIGKILL)" do
    @fully_unkillable_server Path.expand("../support/fully_unkillable_server.sh", __DIR__)

    test "escalates through all three signals when both SIGTERM and SIGABRT are ignored", %{
      id: id
    } do
      pid = start_server(id, executable: @fully_unkillable_server, args: [])
      ServerProcess.launch(pid)
      os_pid = ServerProcess.os_pid(pid)

      # Give the bash traps a moment to fully set up
      Process.sleep(200)
      assert os_process_alive?(os_pid), "process died before stop was called"

      # Both SIGTERM and SIGABRT are trapped, so the full chain runs:
      # SIGTERM (500ms timeout) → SIGABRT (5s timeout) → SIGKILL
      assert ServerProcess.stop(pid, 500) == :escalated
      assert ServerProcess.status(pid) == :stopped

      Process.sleep(100)
      refute os_process_alive?(os_pid)
    end
  end
end
