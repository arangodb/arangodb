defmodule Toast.Deployment.DeploymentLifecycleTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.Controller

  import Toast.DeploymentTestHelpers, only: [make_deployment: 2]

  describe "stop_and_collect/2" do
    test "returns {:ok, empty result} for a controller that never deployed" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-collect-never-deployed")

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      assert diagnostics == %Toast.Diagnostics.Result{}
    end

    test "returns {:ok, empty result} for a controller that was already shut down" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-collect-already-stopped")

      :ok = Controller.shutdown(pid)

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      assert diagnostics == %Toast.Diagnostics.Result{}
    end

    test "returns {:error, ...} when controller process is dead" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-collect-dead")

      GenServer.stop(pid)

      assert {:error, :controller_dead, %Toast.Diagnostics.Result{}} =
               Toast.Deployment.stop_and_collect(deployment)
    end

    test "returns {:error, {:invalid_status, :starting}, partial} when shutdown fails" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      :sys.replace_state(pid, fn state -> %{state | status: :starting} end)

      deployment = make_deployment(pid, id: "test-collect-starting")

      assert {:error, {:invalid_status, :starting}, partial} =
               Toast.Deployment.stop_and_collect(deployment)

      assert is_map(partial)
    end

    test "returns {:ok, empty result} for a cluster controller that never deployed" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      deployment =
        make_deployment(pid,
          id: "test-cluster-never-deployed",
          mode: :cluster,
          config: Toast.Config.load(dump_agency_on_error: false)
        )

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      assert diagnostics == %Toast.Diagnostics.Result{}
    end

    test "returns {:error, :controller_dead, empty result} for a dead cluster controller" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.Cluster, config: Toast.Config.load())

      deployment =
        make_deployment(pid,
          id: "test-cluster-dead",
          mode: :cluster,
          config: Toast.Config.load(dump_agency_on_error: false)
        )

      GenServer.stop(pid)

      assert {:error, :controller_dead, %Toast.Diagnostics.Result{}} =
               Toast.Deployment.stop_and_collect(deployment)
    end

    test "cluster with dump_agency_on_error: false has nil agency_dump" do
      config = Toast.Config.load(dump_agency_on_error: false)

      {:ok, pid} =
        Controller.start_link(mode: Controller.Cluster, config: config)

      deployment =
        make_deployment(pid,
          id: "test-cluster-no-agency-dump",
          mode: :cluster,
          config: config
        )

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      assert diagnostics.agency_dump == nil
    end

    test "config accepts custom debugger module atom" do
      config = Toast.Config.load(debugger: SomeCustomDebugger)
      assert config.debugger == SomeCustomDebugger
    end

    test "debugger: :none skips coredump collection" do
      config = Toast.Config.load(debugger: :none)

      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: config)

      deployment = make_deployment(pid, id: "test-debugger-none", config: config)

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      assert diagnostics.coredump_reports == []
    end
  end

  describe "diagnostics/1" do
    test "returns nil for a controller that never deployed" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-diagnostics-clean")

      assert Toast.Deployment.diagnostics(deployment) == nil
    end

    test "returns nil when controller process is dead" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-diagnostics-dead")

      GenServer.stop(pid)

      assert Toast.Deployment.diagnostics(deployment) == nil
    end
  end

  describe "status/1 with dead controller" do
    test "returns :stopped when controller process has died" do
      {:ok, pid} =
        Controller.start_link(mode: Controller.SingleServer, config: Toast.Config.load())

      deployment = make_deployment(pid, id: "test-status-dead")

      GenServer.stop(pid)

      assert Toast.Deployment.status(deployment) == :stopped
    end
  end
end
