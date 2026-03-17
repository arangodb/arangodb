defmodule Toast.Deployment.IntegrationTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.Controller

  @moduletag :integration

  setup do
    :ok
  end

  describe "single server deployment" do
    test "start, query, and stop" do
      {:ok, deployment} = Toast.Deployment.start_single_server()

      assert %Toast.Deployment{} = deployment
      assert deployment.mode == :single_server
      endpoint = Toast.Deployment.default_endpoint(deployment)
      assert endpoint =~ ~r/^http:\/\/127\.0\.0\.1:\d+$/
      assert is_pid(deployment.controller)

      assert {:ok, %{status: 200, body: body}} =
               Req.get(endpoint <> "/_api/version", retry: false)

      assert body["server"] == "arango"

      assert {:ok, %{status: 201, body: cursor_body}} =
               Req.post(endpoint <> "/_api/cursor",
                 json: %{"query" => "RETURN 1"},
                 retry: false
               )

      assert cursor_body["result"] == [1]

      assert {:ok, _stop_info} = Toast.Deployment.stop(deployment)
    end

    test "server process is gone after stop" do
      {:ok, deployment} = Toast.Deployment.start_single_server()

      server_pid = deployment.controller
      info = Controller.get_info(server_pid)
      assert info.status == :ready

      assert {:ok, _stop_info} = Toast.Deployment.stop(deployment)

      refute Process.alive?(deployment.controller)
    end

    test "custom server args are applied" do
      {:ok, deployment} =
        Toast.Deployment.start_single_server(server_args: %{"server.authentication" => "false"})

      assert {:ok, %{status: 200}} =
               Req.get(Toast.Deployment.default_endpoint(deployment) <> "/_api/version",
                 retry: false
               )

      assert {:ok, _stop_info} = Toast.Deployment.stop(deployment)
    end
  end

  describe "cluster deployment" do
    @tag timeout: 300_000
    test "start, query, and stop" do
      {:ok, deployment} = Toast.Deployment.start_cluster(startup_timeout: 120_000)

      assert %Toast.Deployment{} = deployment
      assert deployment.mode == :cluster
      endpoint = Toast.Deployment.default_endpoint(deployment)
      assert endpoint =~ ~r/^http:\/\/127\.0\.0\.1:\d+$/
      assert is_pid(deployment.controller)

      assert {:ok, %{status: 200, body: body}} =
               Req.get(endpoint <> "/_api/version", retry: false)

      assert body["server"] == "arango"

      assert {:ok, %{status: 201, body: cursor_body}} =
               Req.post(endpoint <> "/_api/cursor",
                 json: %{"query" => "RETURN 1"},
                 retry: false
               )

      assert cursor_body["result"] == [1]

      assert {:ok, _stop_info} = Toast.Deployment.stop(deployment)
    end

    @tag timeout: 300_000
    test "controller process is gone after stop" do
      {:ok, deployment} = Toast.Deployment.start_cluster(startup_timeout: 120_000)

      assert Controller.get_status(deployment.controller) == :ready

      assert {:ok, _stop_info} = Toast.Deployment.stop(deployment)

      refute Process.alive?(deployment.controller)
    end
  end
end
