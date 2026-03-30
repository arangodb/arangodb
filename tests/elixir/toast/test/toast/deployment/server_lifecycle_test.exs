defmodule Toast.Deployment.ServerLifecycleTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.{ServerInstance, ServerLifecycle}

  defp server(overrides) do
    defaults = [id: "s1", role: :single, operational_state: :running, expecting_exit: false]
    struct!(ServerInstance, Keyword.merge(defaults, overrides))
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
end
