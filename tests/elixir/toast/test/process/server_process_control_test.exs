defmodule Toast.Process.ServerProcessControlTest do
  use ExUnit.Case, async: false

  alias Toast.Process.ServerProcess

  import Toast.ServerTestHelpers, only: [cleanup_server: 1, os_process_alive?: 1]

  @fake_server Path.expand("../support/fake_server.sh", __DIR__)

  setup do
    id = "test-ctl-#{System.unique_integer([:positive])}"
    %{id: id}
  end

  defp start_and_launch(id, extra_opts \\ []) do
    opts = Keyword.merge([id: id, executable: @fake_server, args: ["--port", "0"]], extra_opts)
    {:ok, pid} = ServerProcess.start_link(opts)
    on_exit(fn -> cleanup_server(pid) end)
    :ok = ServerProcess.launch(pid)
    pid
  end

  defp start_stopped(id) do
    opts = [id: id, executable: @fake_server, args: ["--port", "0"]]
    {:ok, pid} = ServerProcess.start_link(opts)
    on_exit(fn -> cleanup_server(pid) end)
    pid
  end

  describe "kill/1" do
    test "sends SIGKILL and transitions to :killed", %{id: id} do
      pid = start_and_launch(id)
      os_pid = ServerProcess.os_pid(pid)
      assert :ok = ServerProcess.kill(pid)
      assert ServerProcess.status(pid) == :killed
      Process.sleep(200)
      refute os_process_alive?(os_pid)
    end

    test "on stopped server returns error", %{id: id} do
      pid = start_stopped(id)
      assert {:error, :not_running} = ServerProcess.kill(pid)
    end

    test "does not notify listener", %{id: id} do
      pid = start_and_launch(id, listener: self())
      ServerProcess.kill(pid)
      Process.sleep(500)
      refute_receive {:server_crashed, _, _}
    end
  end

  describe "pause/1" do
    test "sends SIGSTOP and transitions to :paused", %{id: id} do
      pid = start_and_launch(id)
      os_pid = ServerProcess.os_pid(pid)
      assert :ok = ServerProcess.pause(pid)
      assert ServerProcess.status(pid) == :paused
      assert os_process_alive?(os_pid)
    end

    test "on stopped server returns error", %{id: id} do
      pid = start_stopped(id)
      assert {:error, :not_running} = ServerProcess.pause(pid)
    end
  end

  describe "resume/1" do
    test "sends SIGCONT and transitions back to :running", %{id: id} do
      pid = start_and_launch(id)
      :ok = ServerProcess.pause(pid)
      assert ServerProcess.status(pid) == :paused
      assert :ok = ServerProcess.resume(pid)
      assert ServerProcess.status(pid) == :running
    end

    test "on non-paused server returns error", %{id: id} do
      pid = start_and_launch(id)
      assert {:error, :not_paused} = ServerProcess.resume(pid)
    end
  end

  describe "relaunch/2" do
    test "on stopped server re-launches", %{id: id} do
      pid = start_and_launch(id)
      :ok = ServerProcess.stop(pid, 5_000)
      assert ServerProcess.status(pid) == :stopped
      assert :ok = ServerProcess.relaunch(pid)
      assert ServerProcess.status(pid) == :running
      assert is_integer(ServerProcess.os_pid(pid))
    end

    test "on killed server re-launches", %{id: id} do
      pid = start_and_launch(id)
      :ok = ServerProcess.kill(pid)
      Process.sleep(200)
      assert :ok = ServerProcess.relaunch(pid)
      assert ServerProcess.status(pid) == :running
    end

    test "on running server returns error", %{id: id} do
      pid = start_and_launch(id)
      assert {:error, {:already_launched, :running}} = ServerProcess.relaunch(pid)
    end

    test "with extra args merges them with original args", %{id: id} do
      original_args = ["--port", "0", "--mode", "test"]

      opts = [id: id, executable: @fake_server, args: original_args, env: [{"MY_VAR", "val"}]]
      {:ok, pid} = ServerProcess.start_link(opts)
      on_exit(fn -> cleanup_server(pid) end)
      :ok = ServerProcess.launch(pid)
      :ok = ServerProcess.stop(pid, 5_000)

      extra_args = ["--extra-flag", "on"]
      assert :ok = ServerProcess.relaunch(pid, args: extra_args)
      assert ServerProcess.status(pid) == :running

      # Verify the GenServer state has merged args = original ++ extra,
      # and preserved the original executable and env.
      state = :sys.get_state(pid)
      assert state.args == original_args ++ extra_args
      assert state.original_args == original_args
      assert state.executable == @fake_server
      assert state.env == [{"MY_VAR", "val"}]
    end

    test "successive relaunches always merge from original args", %{id: id} do
      original_args = ["--port", "0"]

      opts = [id: id, executable: @fake_server, args: original_args]
      {:ok, pid} = ServerProcess.start_link(opts)
      on_exit(fn -> cleanup_server(pid) end)
      :ok = ServerProcess.launch(pid)
      :ok = ServerProcess.stop(pid, 5_000)

      # First relaunch with extra args
      :ok = ServerProcess.relaunch(pid, args: ["--first"])
      state1 = :sys.get_state(pid)
      assert state1.args == ["--port", "0", "--first"]

      :ok = ServerProcess.stop(pid, 5_000)

      # Second relaunch with different extra args -- should NOT accumulate
      :ok = ServerProcess.relaunch(pid, args: ["--second"])
      state2 = :sys.get_state(pid)
      assert state2.args == ["--port", "0", "--second"]
      assert state2.original_args == original_args
    end
  end

end
