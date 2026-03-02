defmodule Toast.Deployment.ServerLifecycleTest.FromCapture do
  @moduledoc false
  use GenServer

  def init(parent), do: {:ok, parent}

  def handle_call(:capture, from, parent) do
    send(parent, {:from, from})
    {:noreply, parent}
  end
end

defmodule Toast.Deployment.ServerLifecycleTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.{ServerLifecycle, ServerInstance}

  defp server(overrides \\ []) do
    defaults = [id: "s1", role: :single, operational_state: :running, intentional: false]
    struct!(ServerInstance, Keyword.merge(defaults, overrides))
  end

  defp crash_info(overrides \\ []) do
    defaults = [exit_status: 139, signal: 11, timestamp: ~U[2026-01-15 12:00:00Z]]
    Map.new(Keyword.merge(defaults, overrides))
  end

  defp on_crash_ctx(overrides \\ []) do
    %{
      on_crash: Keyword.get(overrides, :on_crash),
      on_event: Keyword.get(overrides, :on_event),
      deployment: Keyword.get(overrides, :deployment, :test_deployment)
    }
  end

  # --- handle_crash/5 ---

  describe "handle_crash/5 with expected crash" do
    test "returns {:expected, updated_map} when server_id is in expected_crashes" do
      timer = make_ref()
      expected = %{"s1" => %{timer: timer, crash_info: nil}}
      info = crash_info()

      assert {:expected, updated} =
               ServerLifecycle.handle_crash("s1", info, expected, nil, on_crash_ctx())

      assert updated["s1"].crash_info == info
      assert updated["s1"].timer == timer
    end

    test "fires on_event with :server_crashed tuple" do
      test_pid = self()
      on_event = fn event -> send(test_pid, {:event, event}) end

      timer = make_ref()
      expected = %{"s1" => %{timer: timer, crash_info: nil}}
      info = crash_info()

      ServerLifecycle.handle_crash("s1", info, expected, nil, on_crash_ctx(on_event: on_event))
      assert_receive {:event, {:server_crashed, "s1", nil, ^info, _timestamp}}
    end

    test "does not fire on_crash for expected crashes" do
      test_pid = self()
      on_crash = fn _deployment, _info -> send(test_pid, :crash_callback) end

      timer = make_ref()
      expected = %{"s1" => %{timer: timer, crash_info: nil}}

      ServerLifecycle.handle_crash(
        "s1",
        crash_info(),
        expected,
        nil,
        on_crash_ctx(on_crash: on_crash)
      )

      refute_receive :crash_callback, 50
    end
  end

  describe "handle_crash/5 with unexpected crash (no server instance)" do
    test "returns :unexpected_crash when server is nil" do
      assert :unexpected_crash =
               ServerLifecycle.handle_crash("s1", crash_info(), %{}, nil, on_crash_ctx())
    end

    test "fires on_crash callback" do
      test_pid = self()
      on_crash = fn deployment, info -> send(test_pid, {:crash, deployment, info}) end
      info = crash_info()

      ServerLifecycle.handle_crash(
        "s1",
        info,
        %{},
        nil,
        on_crash_ctx(on_crash: on_crash, deployment: :my_dep)
      )

      assert_receive {:crash, :my_dep, ^info}
    end

    test "fires on_event callback" do
      test_pid = self()
      on_event = fn event -> send(test_pid, {:event, event}) end
      info = crash_info()

      ServerLifecycle.handle_crash("s1", info, %{}, nil, on_crash_ctx(on_event: on_event))
      assert_receive {:event, {:server_crashed, "s1", nil, ^info, _}}
    end
  end

  describe "handle_crash/5 with intentional server" do
    test "returns :intentional_exit for nil signal" do
      srv = server(intentional: true)
      info = crash_info(signal: nil)

      assert :intentional_exit =
               ServerLifecycle.handle_crash("s1", info, %{}, srv, on_crash_ctx())
    end

    test "returns :intentional_exit for SIGTERM (signal 15)" do
      srv = server(intentional: true)
      info = crash_info(signal: 15)

      assert :intentional_exit =
               ServerLifecycle.handle_crash("s1", info, %{}, srv, on_crash_ctx())
    end

    test "does not fire on_crash for intentional exit" do
      test_pid = self()
      on_crash = fn _dep, _info -> send(test_pid, :crash_callback) end

      srv = server(intentional: true)
      info = crash_info(signal: 15)

      ServerLifecycle.handle_crash("s1", info, %{}, srv, on_crash_ctx(on_crash: on_crash))
      refute_receive :crash_callback, 50
    end

    test "does not fire on_event for intentional exit" do
      test_pid = self()
      on_event = fn event -> send(test_pid, {:event, event}) end

      srv = server(intentional: true)
      info = crash_info(signal: 15)

      ServerLifecycle.handle_crash("s1", info, %{}, srv, on_crash_ctx(on_event: on_event))
      refute_receive {:event, _}, 50
    end

    test "returns :crash_during_intentional_stop for SIGSEGV (signal 11)" do
      srv = server(intentional: true)
      info = crash_info(signal: 11)

      assert :crash_during_intentional_stop =
               ServerLifecycle.handle_crash("s1", info, %{}, srv, on_crash_ctx())
    end

    test "returns :crash_during_intentional_stop for SIGABRT (signal 6)" do
      srv = server(intentional: true)
      info = crash_info(signal: 6)

      assert :crash_during_intentional_stop =
               ServerLifecycle.handle_crash("s1", info, %{}, srv, on_crash_ctx())
    end

    test "fires on_crash for crash during intentional stop" do
      test_pid = self()
      on_crash = fn _dep, info -> send(test_pid, {:crash, info}) end

      srv = server(intentional: true)
      info = crash_info(signal: 11)

      ServerLifecycle.handle_crash("s1", info, %{}, srv, on_crash_ctx(on_crash: on_crash))
      assert_receive {:crash, ^info}
    end
  end

  describe "handle_crash/5 with non-intentional server" do
    test "returns :unexpected_crash for a regular running server" do
      srv = server(intentional: false)
      info = crash_info()

      assert :unexpected_crash =
               ServerLifecycle.handle_crash("s1", info, %{}, srv, on_crash_ctx())
    end

    test "fires both on_crash and on_event" do
      test_pid = self()
      on_crash = fn _dep, info -> send(test_pid, {:crash, info}) end
      on_event = fn event -> send(test_pid, {:event, event}) end
      info = crash_info()

      ServerLifecycle.handle_crash(
        "s1",
        info,
        %{},
        server(),
        on_crash_ctx(on_crash: on_crash, on_event: on_event)
      )

      assert_receive {:crash, ^info}
      assert_receive {:event, {:server_crashed, "s1", nil, ^info, _}}
    end
  end

  # --- expect_crash/4 ---

  describe "expect_crash/4" do
    test "returns {:ok, updated_map} with timer and nil crash_info" do
      srv = server(health_monitor: nil)
      assert {:ok, expected} = ServerLifecycle.expect_crash("s1", 5_000, %{}, srv)

      assert %{timer: timer, crash_info: nil} = expected["s1"]
      assert is_reference(timer)
      Process.cancel_timer(timer)
    end

    test "returns {:error, :already_expected} when server_id already tracked" do
      existing = %{"s1" => %{timer: make_ref(), crash_info: nil}}
      srv = server(health_monitor: nil)

      assert {:error, :already_expected} =
               ServerLifecycle.expect_crash("s1", 5_000, existing, srv)
    end

    test "sends :expect_crash_timeout message after timeout" do
      srv = server(health_monitor: nil)
      {:ok, expected} = ServerLifecycle.expect_crash("s1", 50, %{}, srv)

      assert_receive {:expect_crash_timeout, "s1"}, 200
      # Clean up: timer already fired, but entry still in map
      assert expected["s1"].crash_info == nil
    end

    test "allows expecting different server_ids concurrently" do
      srv = server(health_monitor: nil)
      {:ok, expected} = ServerLifecycle.expect_crash("s1", 5_000, %{}, srv)
      {:ok, expected} = ServerLifecycle.expect_crash("s2", 5_000, expected, srv)

      assert Map.has_key?(expected, "s1")
      assert Map.has_key?(expected, "s2")

      Process.cancel_timer(expected["s1"].timer)
      Process.cancel_timer(expected["s2"].timer)
    end
  end

  # --- verify_crash/4 ---

  describe "verify_crash/4" do
    test "returns {:reply, {:error, :no_expectation}, _} when server_id not tracked" do
      assert {:reply, {:error, :no_expectation}, %{}} =
               ServerLifecycle.verify_crash("s1", 1_000, %{}, self())
    end

    test "returns {:reply, {:ok, crash_info}, _} when crash already recorded" do
      timer = Process.send_after(self(), :noop, 60_000)
      info = crash_info()
      expected = %{"s1" => %{timer: timer, crash_info: info}}

      assert {:reply, {:ok, ^info}, updated} =
               ServerLifecycle.verify_crash("s1", 1_000, expected, self())

      refute Map.has_key?(updated, "s1")
    end

    test "cancels timer when crash already recorded" do
      timer = Process.send_after(self(), :timer_fired, 60_000)
      expected = %{"s1" => %{timer: timer, crash_info: crash_info()}}

      ServerLifecycle.verify_crash("s1", 1_000, expected, self())

      refute_receive :timer_fired, 100
    end

    test "returns {:noreply, _} and schedules check when crash not yet recorded" do
      expected = %{"s1" => %{timer: make_ref(), crash_info: nil}}

      assert {:noreply, ^expected} =
               ServerLifecycle.verify_crash("s1", 1_000, expected, self())

      assert_receive {:verify_crash_check, "s1", _from, _deadline}, 200
    end
  end

  # --- handle_expect_crash_timeout/3 ---

  describe "handle_expect_crash_timeout/3" do
    test "removes entry when crash has not occurred" do
      expected = %{"s1" => %{timer: make_ref(), crash_info: nil}}
      result = ServerLifecycle.handle_expect_crash_timeout("s1", expected, nil)
      assert result == %{}
    end

    test "keeps entry when crash has been recorded" do
      info = crash_info()
      expected = %{"s1" => %{timer: make_ref(), crash_info: info}}
      result = ServerLifecycle.handle_expect_crash_timeout("s1", expected, nil)
      assert result == expected
    end

    test "keeps entry when server_id not found in expected_crashes" do
      expected = %{"other" => %{timer: make_ref(), crash_info: nil}}
      result = ServerLifecycle.handle_expect_crash_timeout("s1", expected, nil)
      assert result == expected
    end

    test "resumes health monitor on timeout when server provided" do
      # We cannot easily verify HealthMonitor.resume is called without mocking,
      # but we can verify no crash when health_monitor is nil.
      srv = server(health_monitor: nil)
      expected = %{"s1" => %{timer: make_ref(), crash_info: nil}}
      result = ServerLifecycle.handle_expect_crash_timeout("s1", expected, srv)
      assert result == %{}
    end
  end

  # --- handle_verify_crash_check/5 ---

  describe "handle_verify_crash_check/5" do
    test "returns {:done, _} with reply when crash is recorded" do
      # We need a real GenServer caller to receive the reply.
      # Use a Task that calls GenServer to get a proper `from`.
      {result, updated} = run_verify_crash_check_with_crash()
      assert result == {:ok, %{exit_status: 139, signal: 11}}
      refute Map.has_key?(updated, "s1")
    end

    test "returns {:done, _} with :no_expectation when entry missing" do
      from = spawn_genserver_from()

      assert {:done, %{}} =
               ServerLifecycle.handle_verify_crash_check("s1", from, 0, %{}, nil)
    end

    test "returns {:wait, _} when deadline not reached and crash not yet recorded" do
      deadline = System.monotonic_time(:millisecond) + 10_000
      expected = %{"s1" => %{timer: make_ref(), crash_info: nil}}
      from = spawn_genserver_from()

      assert {:wait, ^expected} =
               ServerLifecycle.handle_verify_crash_check("s1", from, deadline, expected, nil)

      assert_receive {:verify_crash_check, "s1", ^from, ^deadline}, 200
    end

    test "returns {:done, _} with timeout when deadline passed and no crash" do
      deadline = System.monotonic_time(:millisecond) - 1
      expected = %{"s1" => %{timer: make_ref(), crash_info: nil}}
      {reply, tag} = receive_verify_timeout(deadline, expected)

      assert reply == {:error, :timeout}
      assert tag == :done
      # Entry should be removed from expected_crashes
    end
  end

  # --- require_state/2 ---

  describe "require_state/2" do
    test "returns :ok when state matches" do
      assert :ok = ServerLifecycle.require_state(server(operational_state: :running), :running)
    end

    test "returns error when state does not match" do
      assert {:error, {:unexpected_state, :paused}} =
               ServerLifecycle.require_state(server(operational_state: :paused), :running)
    end

    test "handles nil operational_state" do
      assert {:error, {:unexpected_state, nil}} =
               ServerLifecycle.require_state(server(operational_state: nil), :running)
    end
  end

  # --- require_state_in/2 ---

  describe "require_state_in/2" do
    test "returns :ok when state is in the list" do
      assert :ok =
               ServerLifecycle.require_state_in(server(operational_state: :paused), [
                 :running,
                 :paused
               ])
    end

    test "returns error when state is not in the list" do
      assert {:error, {:unexpected_state, :crashed}} =
               ServerLifecycle.require_state_in(server(operational_state: :crashed), [
                 :running,
                 :paused
               ])
    end

    test "returns error for empty list" do
      assert {:error, {:unexpected_state, :running}} =
               ServerLifecycle.require_state_in(server(operational_state: :running), [])
    end
  end

  # --- notify_event/2 ---

  describe "notify_event/2" do
    test "returns :ok when callback is nil" do
      assert :ok = ServerLifecycle.notify_event(nil, {:some_event})
    end

    test "invokes callback with event" do
      test_pid = self()
      callback = fn event -> send(test_pid, {:got_event, event}) end

      ServerLifecycle.notify_event(callback, {:test_event, "data"})
      assert_receive {:got_event, {:test_event, "data"}}
    end
  end

  # --- notify_crash/3 ---

  describe "notify_crash/3" do
    test "returns :ok when callback is nil" do
      assert :ok = ServerLifecycle.notify_crash(nil, :deployment, :info)
    end

    test "invokes callback with deployment and crash_info" do
      test_pid = self()
      callback = fn dep, info -> send(test_pid, {:got_crash, dep, info}) end

      ServerLifecycle.notify_crash(callback, :my_deployment, :my_info)
      assert_receive {:got_crash, :my_deployment, :my_info}
    end
  end

  # --- print_server_output/2 ---

  describe "print_server_output/2" do
    test "prints each non-empty line with server_id prefix, skipping blank lines" do
      output =
        ExUnit.CaptureIO.capture_io(fn ->
          ServerLifecycle.print_server_output("srv-1", "line1\nline2\n\nline3\n")
        end)

      lines = output |> String.split("\n") |> Enum.reject(&(&1 == ""))
      assert length(lines) == 3
      assert Enum.at(lines, 0) =~ "srv-1 | line1"
      assert Enum.at(lines, 1) =~ "srv-1 | line2"
      assert Enum.at(lines, 2) =~ "srv-1 | line3"
    end

    test "handles empty string without output" do
      output =
        ExUnit.CaptureIO.capture_io(fn ->
          ServerLifecycle.print_server_output("srv-1", "")
        end)

      assert output == ""
    end

    test "handles single line without trailing newline" do
      output =
        ExUnit.CaptureIO.capture_io(fn ->
          ServerLifecycle.print_server_output("srv-1", "hello")
        end)

      assert output =~ "srv-1 | hello"
    end
  end

  # --- Health monitor helpers ---

  describe "suspend_health_monitor/1" do
    test "returns :ok when health_monitor is nil" do
      assert :ok = ServerLifecycle.suspend_health_monitor(%{health_monitor: nil})
    end
  end

  describe "resume_health_monitor/1" do
    test "returns :ok when health_monitor is nil" do
      assert :ok = ServerLifecycle.resume_health_monitor(%{health_monitor: nil})
    end
  end

  describe "stop_health_monitor/1" do
    test "returns :ok when health_monitor is nil" do
      assert :ok = ServerLifecycle.stop_health_monitor(%{health_monitor: nil})
    end
  end

  # --- Helpers for testing GenServer.reply interactions ---

  # Spawns a GenServer that captures the `from` tuple and sends it back,
  # so we can pass it to handle_verify_crash_check.
  defp spawn_genserver_from do
    parent = self()

    {:ok, pid} =
      GenServer.start(Toast.Deployment.ServerLifecycleTest.FromCapture, parent)

    # Make a call that will capture `from` and send it to us
    Task.async(fn -> GenServer.call(pid, :capture, 5_000) end)

    receive do
      {:from, from} -> from
    after
      1_000 -> raise "timeout waiting for from"
    end
  end

  defp run_verify_crash_check_with_crash do
    parent = self()
    info = %{exit_status: 139, signal: 11}
    timer = Process.send_after(self(), :noop, 60_000)

    task =
      Task.async(fn ->
        {:ok, pid} =
          GenServer.start(Toast.Deployment.ServerLifecycleTest.FromCapture, parent)

        GenServer.call(pid, :capture, 5_000)
      end)

    from =
      receive do
        {:from, f} -> f
      after
        1_000 -> raise "timeout"
      end

    expected = %{"s1" => %{timer: timer, crash_info: info}}

    {tag, updated} =
      ServerLifecycle.handle_verify_crash_check("s1", from, 0, expected, nil)

    result = Task.await(task, 1_000)
    assert tag == :done
    {result, updated}
  end

  defp receive_verify_timeout(deadline, expected) do
    parent = self()

    task =
      Task.async(fn ->
        {:ok, pid} =
          GenServer.start(Toast.Deployment.ServerLifecycleTest.FromCapture, parent)

        GenServer.call(pid, :capture, 5_000)
      end)

    from =
      receive do
        {:from, f} -> f
      after
        1_000 -> raise "timeout"
      end

    {tag, _updated} =
      ServerLifecycle.handle_verify_crash_check("s1", from, deadline, expected, nil)

    reply = Task.await(task, 1_000)
    {reply, tag}
  end
end
