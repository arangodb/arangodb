defmodule Toast.Deployment.ResolveTargetTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.{ClusterController, SingleServerController, ServerInstance}

  # resolve_target/2 is private in both controllers. We test it through
  # the public control operations (stop_server, kill_server, etc.) which
  # delegate to resolve_target for target resolution. By examining the
  # error shapes returned, we verify correct target resolution behavior.
  #
  # When resolve_target succeeds, the operation then checks operational_state.
  # When resolve_target fails, we get the resolution error directly.

  # --- ClusterController target resolution ---

  describe "ClusterController string target" do
    setup [:start_cluster_with_servers]

    test "known server_id resolves successfully", %{ctrl: ctrl} do
      # stop_server will resolve "dbserver-0", then fail on operational_state
      # because server is in nil state, not :running
      result = ClusterController.stop_server(ctrl, "dbserver-0")
      # Resolution succeeded; we get an operational_state error, not a target error
      assert {:error, {:unexpected_state, _}} = result
    end

    test "unknown server_id returns :not_found", %{ctrl: ctrl} do
      assert {:error, :not_found} = ClusterController.stop_server(ctrl, "nonexistent")
    end
  end

  describe "ClusterController role target" do
    setup [:start_cluster_with_servers]

    test "role: :dbserver resolves all dbservers", %{ctrl: ctrl} do
      # Set to :paused so stop_server (which requires :running) gives an
      # operational_state error, proving target resolution succeeded.
      set_operational_state(ctrl, "dbserver-0", :paused)
      set_operational_state(ctrl, "dbserver-1", :paused)
      set_operational_state(ctrl, "dbserver-2", :paused)

      result = ClusterController.stop_server(ctrl, role: :dbserver)
      assert {:error, {:unexpected_state, :paused}} = result
    end

    test "unknown role returns :no_servers_for_role", %{ctrl: ctrl} do
      # :agent role has no servers in our test setup
      assert {:error, {:no_servers_for_role, :agent}} = ClusterController.stop_server(ctrl, role: :agent)
    end
  end

  describe "ClusterController role+index target" do
    setup [:start_cluster_with_servers]

    test "role+index resolves single server", %{ctrl: ctrl} do
      set_operational_state(ctrl, "coordinator-0", :paused)

      result = ClusterController.stop_server(ctrl, role: :coordinator, index: 0)
      # Resolution succeeded (got operational state error, not target error)
      assert {:error, {:unexpected_state, :paused}} = result
    end

    test "out-of-bounds index returns :no_server_at_index", %{ctrl: ctrl} do
      result = ClusterController.stop_server(ctrl, role: :coordinator, index: 5)
      assert {:error, {:no_server_at_index, :coordinator, 5}} = result
    end
  end

  describe "ClusterController cluster_id target" do
    setup [:start_cluster_with_servers]

    test "known cluster_id resolves to correct server", %{ctrl: ctrl} do
      inject_cluster_id_mapping(ctrl, %{
        "dbserver-0" => "PRMR-abc123",
        "dbserver-1" => "PRMR-def456"
      })

      set_operational_state(ctrl, "dbserver-0", :paused)

      result = ClusterController.stop_server(ctrl, cluster_id: "PRMR-abc123")
      # Resolution succeeded (got operational state error)
      assert {:error, {:unexpected_state, :paused}} = result
    end

    test "unknown cluster_id returns :not_found", %{ctrl: ctrl} do
      inject_cluster_id_mapping(ctrl, %{"dbserver-0" => "PRMR-abc123"})

      result = ClusterController.stop_server(ctrl, cluster_id: "PRMR-unknown")
      assert {:error, :not_found} = result
    end
  end

  describe "ClusterController invalid target" do
    setup [:start_cluster_with_servers]

    test "invalid target format returns :invalid_target", %{ctrl: ctrl} do
      result = ClusterController.stop_server(ctrl, 12345)
      assert {:error, {:invalid_target, 12345}} = result
    end

    test "empty keyword list returns :invalid_target", %{ctrl: ctrl} do
      result = ClusterController.stop_server(ctrl, [])
      assert {:error, {:invalid_target, []}} = result
    end
  end

  # --- SingleServerController target resolution ---

  describe "SingleServerController string target" do
    test "matching server_id resolves successfully" do
      id = "ssc-resolve-#{System.unique_integer([:positive])}"
      {:ok, ctrl} = SingleServerController.start_link(config: Toast.Config.load(), id: id)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      # The server starts with operational_state nil, so stop_server will fail
      # on the operational_state check, proving resolution succeeded
      result = SingleServerController.stop_server(ctrl, id)
      assert {:error, {:unexpected_state, _}} = result
    end

    test "non-matching server_id returns :not_found" do
      id = "ssc-resolve-#{System.unique_integer([:positive])}"
      {:ok, ctrl} = SingleServerController.start_link(config: Toast.Config.load(), id: id)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      assert {:error, :not_found} = SingleServerController.stop_server(ctrl, "wrong-id")
    end
  end

  describe "SingleServerController role target" do
    test "role: :single resolves to the server" do
      id = "ssc-role-#{System.unique_integer([:positive])}"
      {:ok, ctrl} = SingleServerController.start_link(config: Toast.Config.load(), id: id)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      # Resolves successfully; fails on operational_state check
      result = SingleServerController.stop_server(ctrl, role: :single)
      assert {:error, {:unexpected_state, _}} = result
    end

    test "other roles return :no_servers_for_role" do
      id = "ssc-role-#{System.unique_integer([:positive])}"
      {:ok, ctrl} = SingleServerController.start_link(config: Toast.Config.load(), id: id)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      assert {:error, {:no_servers_for_role, :dbserver}} = SingleServerController.stop_server(ctrl, role: :dbserver)
      assert {:error, {:no_servers_for_role, :coordinator}} = SingleServerController.stop_server(ctrl, role: :coordinator)
      assert {:error, {:no_servers_for_role, :agent}} = SingleServerController.stop_server(ctrl, role: :agent)
    end
  end

  describe "SingleServerController invalid target" do
    test "invalid target format returns :invalid_target" do
      id = "ssc-invalid-#{System.unique_integer([:positive])}"
      {:ok, ctrl} = SingleServerController.start_link(config: Toast.Config.load(), id: id)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      assert {:error, {:invalid_target, 42}} = SingleServerController.stop_server(ctrl, 42)
    end
  end

  # --- Through Deployment public API ---

  describe "Deployment.stop_server target resolution" do
    test "cluster deployment with role target" do
      {:ok, ctrl} = ClusterController.start_link(config: Toast.Config.load())
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      inject_cluster_servers(ctrl, %{
        "dbserver-0" => %ServerInstance{id: "dbserver-0", role: :dbserver, operational_state: :paused},
        "coordinator-0" => %ServerInstance{id: "coordinator-0", role: :coordinator, operational_state: :running}
      })

      deployment = cluster_deployment(ctrl)

      # Role target through Deployment API
      result = Toast.Deployment.stop_server(deployment, role: :dbserver)
      assert {:error, {:unexpected_state, :paused}} = result
    end

    test "single server deployment with role: :single" do
      id = "ssc-deploy-api-#{System.unique_integer([:positive])}"
      {:ok, ctrl} = SingleServerController.start_link(config: Toast.Config.load(), id: id)
      on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

      deployment = %Toast.Deployment{
        id: id,
        mode: :single_server,
        config: Toast.Config.load(),
        controller: ctrl,
        endpoint: "http://127.0.0.1:0",
        work_dir: "/tmp/test"
      }

      result = Toast.Deployment.stop_server(deployment, role: :single)
      assert {:error, {:unexpected_state, _}} = result
    end
  end

  # --- Setup helpers ---

  defp start_cluster_with_servers(_context) do
    {:ok, ctrl} = ClusterController.start_link(config: Toast.Config.load())

    servers = %{
      "dbserver-0" => %ServerInstance{id: "dbserver-0", role: :dbserver},
      "dbserver-1" => %ServerInstance{id: "dbserver-1", role: :dbserver},
      "dbserver-2" => %ServerInstance{id: "dbserver-2", role: :dbserver},
      "coordinator-0" => %ServerInstance{id: "coordinator-0", role: :coordinator}
    }

    inject_cluster_servers(ctrl, servers)

    on_exit(fn -> if Process.alive?(ctrl), do: GenServer.stop(ctrl) end)

    %{ctrl: ctrl}
  end

  defp inject_cluster_servers(ctrl, servers) do
    :sys.replace_state(ctrl, fn state ->
      agents = for {id, s} <- servers, s.role == :agent, do: id
      dbservers = for {id, s} <- servers, s.role == :dbserver, do: id
      coordinators = for {id, s} <- servers, s.role == :coordinator, do: id

      %{state |
        servers: servers,
        agents: agents,
        dbservers: dbservers,
        coordinators: coordinators,
        status: :ready
      }
    end)
  end

  defp inject_cluster_id_mapping(ctrl, mapping) do
    :sys.replace_state(ctrl, fn state ->
      %{state | cluster_id_mapping: mapping}
    end)
  end

  defp set_operational_state(ctrl, server_id, op_state) do
    :sys.replace_state(ctrl, fn state ->
      server = state.servers[server_id]
      updated = %{server | operational_state: op_state}
      %{state | servers: Map.put(state.servers, server_id, updated)}
    end)
  end

  defp cluster_deployment(ctrl) do
    %Toast.Deployment{
      id: "test-resolve",
      mode: :cluster,
      config: Toast.Config.load(),
      controller: ctrl,
      endpoint: "http://127.0.0.1:0",
      work_dir: "/tmp/test"
    }
  end
end
