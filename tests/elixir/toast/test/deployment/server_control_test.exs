defmodule Toast.Deployment.ServerControlTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment

  describe "control API exists" do
    test "stop_server/2 function is exported" do
      assert function_exported?(Deployment, :stop_server, 2)
    end

    test "kill_server/2 function is exported" do
      assert function_exported?(Deployment, :kill_server, 2)
    end

    test "pause_server/2 function is exported" do
      assert function_exported?(Deployment, :pause_server, 2)
    end

    test "resume_server/2 function is exported" do
      assert function_exported?(Deployment, :resume_server, 2)
    end

    test "restart_server/3 function is exported" do
      assert function_exported?(Deployment, :restart_server, 3)
    end

    test "start_server/3 function is exported" do
      assert function_exported?(Deployment, :start_server, 3)
    end
  end

  describe "controller_call_control with dead controller" do
    test "returns {:error, :controller_not_available} for dead controller" do
      deployment = %Deployment{
        id: "test",
        mode: :single_server,
        config: %Toast.Config{},
        controller: spawn(fn -> :ok end),
        endpoint: "http://localhost:1",
        work_dir: "/tmp"
      }

      Process.sleep(50)
      assert {:error, :controller_not_available} = Deployment.stop_server(deployment, "s1")
    end
  end
end
