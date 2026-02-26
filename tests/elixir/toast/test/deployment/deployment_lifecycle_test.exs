defmodule Toast.Deployment.DeploymentLifecycleTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.SingleServerController, as: Controller
  alias Toast.Deployment.ClusterController

  defp make_deployment(pid, opts) do
    mode = Keyword.get(opts, :mode, :single_server)

    %Toast.Deployment{
      id: Keyword.get(opts, :id, "test-lifecycle"),
      mode: mode,
      config: Keyword.get(opts, :config, Toast.Config.load()),
      endpoint: "http://127.0.0.1:0",
      controller: pid,
      work_dir: "/tmp/toast-test"
    }
  end

  describe "stop_and_collect/2" do
    test "returns {:ok, empty map} for a controller that never deployed" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-collect-never-deployed")

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      assert diagnostics == %{}
    end

    test "returns {:ok, empty map} for a controller that was already shut down" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-collect-already-stopped")

      :ok = Controller.shutdown(pid)

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      assert diagnostics == %{}
    end

    test "returns {:error, ...} when controller process is dead" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-collect-dead")

      GenServer.stop(pid)

      assert {:error, :controller_dead, %{}} =
               Toast.Deployment.stop_and_collect(deployment)
    end

    # T1: shutdown returns error when controller is in :starting state
    test "returns {:error, {:invalid_status, :starting}, partial} when shutdown fails" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())

      # Force the controller into :starting status via deploy on a non-existent binary.
      # deploy will fail, leaving status as :failed. But we need :starting.
      # Instead, we can call shutdown while status is :starting by racing deploy,
      # but that's fragile. The cleaner route: directly test that calling shutdown
      # on a controller in a non-shuttable state returns the error through
      # stop_and_collect. We can set the state to :starting by sending a deploy
      # that will block. However, a simpler approach: since the shutdown handler
      # for status :starting returns {:error, {:invalid_status, :starting}},
      # we can verify this via GenServer state manipulation.
      #
      # The most reliable approach: use :sys.replace_state to force :starting status.
      :sys.replace_state(pid, fn state -> %{state | status: :starting} end)

      deployment = make_deployment(pid, id: "test-collect-starting")

      assert {:error, {:invalid_status, :starting}, partial} =
               Toast.Deployment.stop_and_collect(deployment)

      assert is_map(partial)
    end

    # T2: cluster controller lifecycle — never deployed
    test "returns {:ok, empty map} for a cluster controller that never deployed" do
      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load())

      deployment =
        make_deployment(pid,
          id: "test-cluster-never-deployed",
          mode: :cluster,
          config: Toast.Config.load(dump_agency_on_error: false)
        )

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      assert diagnostics == %{}
    end

    # T2: cluster controller lifecycle — dead controller
    test "returns {:error, :controller_dead, empty map} for a dead cluster controller" do
      {:ok, pid} = ClusterController.start_link(config: Toast.Config.load())

      deployment =
        make_deployment(pid,
          id: "test-cluster-dead",
          mode: :cluster,
          config: Toast.Config.load(dump_agency_on_error: false)
        )

      GenServer.stop(pid)

      assert {:error, :controller_dead, %{}} =
               Toast.Deployment.stop_and_collect(deployment)
    end

    # T9: dump_agency_on_error: false skips agency dump for cluster
    test "cluster with dump_agency_on_error: false omits agency_dump from result" do
      config = Toast.Config.load(dump_agency_on_error: false)
      {:ok, pid} = ClusterController.start_link(config: config)

      deployment =
        make_deployment(pid,
          id: "test-cluster-no-agency-dump",
          mode: :cluster,
          config: config
        )

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      refute Map.has_key?(diagnostics, :agency_dump)
    end

    test "config accepts custom debugger module atom" do
      config = Toast.Config.load(debugger: SomeCustomDebugger)
      assert config.debugger == SomeCustomDebugger
    end

    test "debugger: :none skips coredump collection" do
      config = Toast.Config.load(debugger: :none)
      {:ok, pid} = Controller.start_link(config: config)
      deployment = make_deployment(pid, id: "test-debugger-none", config: config)

      assert {:ok, diagnostics} = Toast.Deployment.stop_and_collect(deployment)
      refute Map.has_key?(diagnostics, :coredump_reports)
    end
  end

  describe "diagnostics/1" do
    test "returns nil for a controller that never deployed" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-diagnostics-clean")

      assert Toast.Deployment.diagnostics(deployment) == nil
    end

    test "returns nil when controller process is dead" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-diagnostics-dead")

      GenServer.stop(pid)

      assert Toast.Deployment.diagnostics(deployment) == nil
    end
  end

  describe "status/1 with dead controller" do
    test "returns :stopped when controller process has died" do
      {:ok, pid} = Controller.start_link(config: Toast.Config.load())
      deployment = make_deployment(pid, id: "test-status-dead")

      GenServer.stop(pid)

      assert Toast.Deployment.status(deployment) == :stopped
    end
  end
end
