defmodule Toast.Deployment.IntegrationTest do
  use ExUnit.Case, async: false

  alias Toast.Deployment.Controller

  @moduletag :integration

  setup do
    dir =
      Path.join(System.tmp_dir!(), "toast/integration-test-#{System.unique_integer([:positive])}")

    {:ok, deployment_dir: dir}
  end

  describe "single server deployment" do
    test "start, query, and stop", %{deployment_dir: dir} do
      {:ok, deployment} = Toast.Deployment.start_single_server(dir)

      assert %Toast.Deployment{} = deployment
      refute Toast.Deployment.cluster?(deployment)
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

    test "server process is gone after stop", %{deployment_dir: dir} do
      {:ok, deployment} = Toast.Deployment.start_single_server(dir)

      server_pid = deployment.controller
      info = Controller.get_info(server_pid)
      assert info.status == :ready

      assert {:ok, _stop_info} = Toast.Deployment.stop(deployment)

      refute Process.alive?(deployment.controller)
    end

    test "custom server args are applied", %{deployment_dir: dir} do
      config = Toast.Deployment.Config.new(server_args: %{"server.authentication" => "false"})
      {:ok, deployment} = Toast.Deployment.start_single_server(config, dir)

      assert {:ok, %{status: 200}} =
               Req.get(Toast.Deployment.default_endpoint(deployment) <> "/_api/version",
                 retry: false
               )

      assert {:ok, _stop_info} = Toast.Deployment.stop(deployment)
    end
  end

  describe "cluster deployment" do
    @tag timeout: 300_000
    test "start, query, and stop", %{deployment_dir: dir} do
      config = Toast.Deployment.Config.new(cluster: true, startup_timeout: 120_000)
      {:ok, deployment} = Toast.Deployment.start(config, dir)

      assert %Toast.Deployment{} = deployment
      assert Toast.Deployment.cluster?(deployment)
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
    test "controller process is gone after stop", %{deployment_dir: dir} do
      config = Toast.Deployment.Config.new(cluster: true, startup_timeout: 120_000)
      {:ok, deployment} = Toast.Deployment.start(config, dir)

      assert Controller.get_status(deployment.controller) == :ready

      assert {:ok, _stop_info} = Toast.Deployment.stop(deployment)

      refute Process.alive?(deployment.controller)
    end
  end
end
