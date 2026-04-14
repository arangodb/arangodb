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

  describe "authentication" do
    alias Toast.Client
    alias Toast.JWT

    test "HS256: authenticated client works, unauthenticated returns 401", %{
      deployment_dir: dir
    } do
      config = Toast.Deployment.Config.new(authentication: true)
      {:ok, deployment} = Toast.Deployment.start_single_server(config, dir)

      assert %JWT.Provider{} = deployment.jwt_provider

      {:ok, client} = Toast.Deployment.client_for_role(deployment, :single, 0)
      assert {:ok, %{status: 200}} = Client.get(client, "/_api/version")

      endpoint = Toast.Deployment.default_endpoint(deployment)
      assert {:ok, %{status: 401}} = Req.get(endpoint <> "/_api/version", retry: false)

      assert {:ok, _} = Toast.Deployment.stop(deployment)
    end

    test "ES256: authenticated client works", %{deployment_dir: dir} do
      config = Toast.Deployment.Config.new(authentication: true, jwt_algorithm: :ecdsa)

      {:ok, deployment} = Toast.Deployment.start_single_server(config, dir)

      {:ok, client} = Toast.Deployment.client_for_role(deployment, :single, 0)
      assert {:ok, %{status: 200, body: body}} = Client.get(client, "/_api/version")
      assert body["server"] == "arango"

      assert {:ok, _} = Toast.Deployment.stop(deployment)
    end

    test "short-lived static token expires; provider-backed client keeps working", %{
      deployment_dir: dir
    } do
      config = Toast.Deployment.Config.new(authentication: true)
      {:ok, deployment} = Toast.Deployment.start_single_server(config, dir)

      # Provider-backed client mints per-request — no staleness concern.
      {:ok, live_client} = Toast.Deployment.client_for_role(deployment, :single, 0)

      # Static short-lived token — must expire on its own.
      short = JWT.Provider.create_token(deployment.jwt_provider, :superuser, lifetime: 1)
      endpoint = Toast.Deployment.default_endpoint(deployment)
      static_client = Client.new(endpoint) |> Client.with_auth({:jwt, short})

      assert {:ok, %{status: 200}} = Client.get(static_client, "/_api/version")

      # Wait past expiry plus a small slack for clock drift.
      Process.sleep(2_500)

      assert {:ok, %{status: 401}} = Client.get(static_client, "/_api/version")
      # Live client still works.
      assert {:ok, %{status: 200}} = Client.get(live_client, "/_api/version")

      assert {:ok, _} = Toast.Deployment.stop(deployment)
    end

    test "token with wrong issuer is rejected", %{deployment_dir: dir} do
      config = Toast.Deployment.Config.new(authentication: true)
      {:ok, deployment} = Toast.Deployment.start_single_server(config, dir)

      bad =
        JWT.Provider.create_token(deployment.jwt_provider, :superuser,
          extra_claims: %{"iss" => "wrong"}
        )

      endpoint = Toast.Deployment.default_endpoint(deployment)
      client = Client.new(endpoint) |> Client.with_auth({:jwt, bad})
      assert {:ok, %{status: 401}} = Client.get(client, "/_api/version")

      assert {:ok, _} = Toast.Deployment.stop(deployment)
    end

    @tag timeout: 300_000
    test "cluster: authenticated client works end-to-end", %{deployment_dir: dir} do
      config =
        Toast.Deployment.Config.new(
          cluster: true,
          startup_timeout: 120_000,
          authentication: true
        )

      {:ok, deployment} = Toast.Deployment.start(config, dir)

      {:ok, client} = Toast.Deployment.client_for_role(deployment, :coordinator, 0)
      assert {:ok, %{status: 200}} = Client.get(client, "/_api/version")

      assert {:ok, _} = Toast.Deployment.stop(deployment)
    end
  end
end
