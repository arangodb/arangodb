defmodule Toast.Deployment.IntegrationTest do
  use ExUnit.Case, async: false

  @moduletag :integration

  setup do
    :ok
  end

  describe "single server deployment" do
    test "start, query, and stop" do
      {:ok, deployment} = Toast.Deployment.start(:single_server)

      assert %Toast.Deployment{} = deployment
      assert deployment.mode == :single_server
      assert deployment.endpoint =~ ~r/^http:\/\/127\.0\.0\.1:\d+$/
      assert is_pid(deployment.controller)

      # Health check — /_api/version should respond
      assert {:ok, %{status: 200, body: body}} =
               Req.get(deployment.endpoint <> "/_api/version", retry: false)

      assert body["server"] == "arango"

      # Run an AQL query via /_api/cursor
      assert {:ok, %{status: 201, body: cursor_body}} =
               Req.post(deployment.endpoint <> "/_api/cursor",
                 json: %{"query" => "RETURN 1"},
                 retry: false
               )

      assert cursor_body["result"] == [1]

      # Stop deployment
      assert :ok = Toast.Deployment.stop(deployment)
    end

    test "server process is gone after stop" do
      {:ok, deployment} = Toast.Deployment.start(:single_server)

      # Get the OS pid before stopping
      server_pid = deployment.controller
      info = Toast.Deployment.SingleServerController.get_info(server_pid)
      assert info.status == :ready

      assert :ok = Toast.Deployment.stop(deployment)

      # The controller process should be terminated
      refute Process.alive?(deployment.controller)
    end

    test "custom server args are applied" do
      {:ok, deployment} =
        Toast.Deployment.start(:single_server,
          server_args: %{"server.authentication" => "false"}
        )

      # Should be accessible without auth
      assert {:ok, %{status: 200}} =
               Req.get(deployment.endpoint <> "/_api/version", retry: false)

      assert :ok = Toast.Deployment.stop(deployment)
    end
  end

  describe "cluster deployment" do
    @tag timeout: 300_000
    test "start, query, and stop" do
      {:ok, deployment} = Toast.Deployment.start(:cluster, startup_timeout: 120_000)

      assert %Toast.Deployment{} = deployment
      assert deployment.mode == :cluster
      assert deployment.endpoint =~ ~r/^http:\/\/127\.0\.0\.1:\d+$/
      assert is_pid(deployment.controller)
      assert is_map(deployment.servers)
      assert map_size(deployment.servers) == 7

      # Verify topology: 3 agents, 3 dbservers, 1 coordinator
      roles = deployment.servers |> Map.values() |> Enum.map(& &1.role)
      assert Enum.count(roles, &(&1 == :agent)) == 3
      assert Enum.count(roles, &(&1 == :dbserver)) == 3
      assert Enum.count(roles, &(&1 == :coordinator)) == 1

      # Query via coordinator endpoint
      assert {:ok, %{status: 200, body: body}} =
               Req.get(deployment.endpoint <> "/_api/version", retry: false)

      assert body["server"] == "arango"

      # Run an AQL query
      assert {:ok, %{status: 201, body: cursor_body}} =
               Req.post(deployment.endpoint <> "/_api/cursor",
                 json: %{"query" => "RETURN 1"},
                 retry: false
               )

      assert cursor_body["result"] == [1]

      assert :ok = Toast.Deployment.stop(deployment)
    end

    @tag timeout: 300_000
    test "controller process is gone after stop" do
      {:ok, deployment} = Toast.Deployment.start(:cluster, startup_timeout: 120_000)

      assert Toast.Deployment.ClusterController.get_status(deployment.controller) == :ready

      assert :ok = Toast.Deployment.stop(deployment)

      refute Process.alive?(deployment.controller)
    end
  end
end
