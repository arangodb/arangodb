defmodule Toast.Deployment.Controller.HelpersTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.Controller.Helpers
  alias Toast.Deployment.{Controller, ServerInstance}
  alias Toast.Process.ServerProcess

  import Toast.ServerTestHelpers, only: [cleanup_server: 1]

  @fake_server Path.expand("../support/fake_server.sh", __DIR__)
  @unkillable_server Path.expand("../support/unkillable_server.sh", __DIR__)

  # --- stop_server_process ---

  describe "stop_server_process/4" do
    setup do
      id = "helpers-stop-#{System.unique_integer([:positive])}"

      {:ok, server_pid} =
        ServerProcess.start_link(id: id, executable: @fake_server, args: ["--port", "0"])

      :ok = ServerProcess.launch(server_pid)
      os_pid = ServerProcess.os_pid(server_pid)

      on_exit(fn -> cleanup_server(server_pid) end)

      state = %Controller.State{
        config: Toast.Config.load(),
        mode: Controller.SingleServer,
        id: id,
        servers: %{
          id => %ServerInstance{
            id: id,
            role: :single,
            server_pid: server_pid,
            pid: os_pid,
            log_file: "/tmp/test.log"
          }
        }
      }

      %{state: state, id: id, server_pid: server_pid}
    end

    test "returns :ok for graceful stop", %{state: state, id: id} do
      assert :ok = Helpers.stop_server_process(state, id, 5_000)
    end

    test "invokes on_event callback with server_stopped tuple", %{state: state, id: id} do
      test_pid = self()
      on_event = fn event -> send(test_pid, {:event, event}) end

      Helpers.stop_server_process(state, id, 5_000, on_event: on_event)

      assert_receive {:event, {:server_stopped, ^id, _os_pid, nil, %DateTime{}}}, 1_000
    end

    test "returns {:escalated, info} when escalation occurs", %{id: id} do
      # Start an unkillable server that requires escalation
      {:ok, unkillable_pid} =
        ServerProcess.start_link(id: id <> "-esc", executable: @unkillable_server, args: [])

      :ok = ServerProcess.launch(unkillable_pid)
      os_pid = ServerProcess.os_pid(unkillable_pid)
      on_exit(fn -> cleanup_server(unkillable_pid) end)

      # Give the bash trap a moment to set up
      Process.sleep(200)

      state = %Controller.State{
        config: Toast.Config.load(),
        mode: Controller.SingleServer,
        id: id,
        servers: %{
          id => %ServerInstance{
            id: id,
            role: :single,
            server_pid: unkillable_pid,
            pid: os_pid,
            log_file: "/tmp/escalated.log"
          }
        }
      }

      result = Helpers.stop_server_process(state, id, 500)
      assert {:escalated, info} = result
      assert info.server_id == id
      assert info.os_pid == os_pid
      assert info.log_file == "/tmp/escalated.log"
    end

    test "returns :ok when server has no server_pid" do
      state = %Controller.State{
        config: Toast.Config.load(),
        mode: Controller.SingleServer,
        id: "no-pid",
        servers: %{
          "no-pid" => %ServerInstance{id: "no-pid", role: :single, server_pid: nil}
        }
      }

      assert :ok = Helpers.stop_server_process(state, "no-pid", 5_000)
    end
  end

  # --- record_shutdown_escalations ---

  describe "record_shutdown_escalations/2" do
    test "no-op for empty list" do
      # Should not raise or have any side effect
      assert :ok = Helpers.record_shutdown_escalations("test-id", [])
    end

    test "records timeout_kill with :shutdown_timeout source for non-empty list" do
      # ProcessHistory.record_timeout_kill casts to __MODULE__ (the default name),
      # so we need it running under that name for this test.
      ensure_process_history_running()

      escalated = [
        %{server_id: "s1", os_pid: 1001, log_file: "/tmp/s1.log"},
        %{server_id: "s2", os_pid: 1002, log_file: "/tmp/s2.log"}
      ]

      Helpers.record_shutdown_escalations("test-deploy", escalated)

      # ProcessHistory is async (cast), give it a moment
      Process.sleep(50)

      kills = ToastTest.ProcessHistory.timeout_kills()
      assert length(kills) == 1

      [kill] = kills
      assert kill.source == :shutdown_timeout
      assert kill.reason =~ "SIGTERM"
      assert length(kill.servers) == 2
    end
  end

  # --- handle_deploy_failure ---

  describe "handle_deploy_failure/3" do
    test "records startup_timeout when reason is :timeout" do
      ensure_process_history_running()

      # State with no running servers (no server_pids to abort)
      state = %Controller.State{
        config: Toast.Config.load(),
        mode: Controller.SingleServer,
        id: "deploy-fail-timeout",
        servers: %{
          "s1" => %ServerInstance{id: "s1", role: :single, server_pid: nil}
        }
      }

      rollback_fn = fn s, _reason -> %{s | status: :failed} end

      result = Helpers.handle_deploy_failure(state, :timeout, rollback_fn)

      assert {:error, :timeout, rolled_back} = result
      assert rolled_back.status == :failed

      Process.sleep(50)

      kills = ToastTest.ProcessHistory.timeout_kills()
      timeout_kills = Enum.filter(kills, &(&1.source == :startup_timeout))
      assert length(timeout_kills) == 1
      assert hd(timeout_kills).reason =~ "Startup timeout"
    end

    test "does NOT record timeout_kill for non-timeout reasons" do
      ensure_process_history_running()
      # Clear any previous events
      ToastTest.ProcessHistory.clear()
      Process.sleep(50)

      state = %Controller.State{
        config: Toast.Config.load(),
        mode: Controller.SingleServer,
        id: "deploy-fail-other",
        servers: %{
          "s1" => %ServerInstance{id: "s1", role: :single, server_pid: nil}
        }
      }

      rollback_fn = fn s, _reason -> %{s | status: :failed} end

      result = Helpers.handle_deploy_failure(state, :connection_refused, rollback_fn)

      assert {:error, :connection_refused, rolled_back} = result
      assert rolled_back.status == :failed

      Process.sleep(50)

      kills = ToastTest.ProcessHistory.timeout_kills()
      assert kills == []
    end

    test "invokes the rollback function with state and reason" do
      ensure_process_history_running()

      state = %Controller.State{
        config: Toast.Config.load(),
        mode: Controller.SingleServer,
        id: "deploy-fail-rollback",
        servers: %{}
      }

      test_pid = self()

      rollback_fn = fn s, reason ->
        send(test_pid, {:rollback_called, reason})
        %{s | status: :failed, error: reason}
      end

      Helpers.handle_deploy_failure(state, :some_error, rollback_fn)

      assert_receive {:rollback_called, :some_error}, 1_000
    end
  end

  # Ensure ProcessHistory is running under its default name for tests
  # that exercise code calling ProcessHistory.record_timeout_kill/3
  # (which casts to the __MODULE__ name).
  defp ensure_process_history_running do
    case Process.whereis(ToastTest.ProcessHistory) do
      nil ->
        {:ok, pid} = ToastTest.ProcessHistory.start_link(name: ToastTest.ProcessHistory)
        # Clear any pre-existing state
        ToastTest.ProcessHistory.clear()
        Process.sleep(50)
        pid

      _pid ->
        ToastTest.ProcessHistory.clear()
        Process.sleep(50)
        :already_running
    end
  end
end
