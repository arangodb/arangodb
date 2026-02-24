defmodule ToastTest.ServerIdMappingTest do
  use ExUnit.Case, async: true

  test "cluster_id returns error for single server deployment" do
    deployment = %Toast.Deployment{
      id: "test-1",
      mode: :single_server,
      config: Toast.Config.load(),
      controller: self(),
      endpoint: "http://localhost:8529",
      work_dir: "/tmp/test"
    }

    assert {:error, :not_cluster} = Toast.Deployment.cluster_id(deployment, "server-0")
  end

  test "server_by_cluster_id returns error for single server deployment" do
    deployment = %Toast.Deployment{
      id: "test-1",
      mode: :single_server,
      config: Toast.Config.load(),
      controller: self(),
      endpoint: "http://localhost:8529",
      work_dir: "/tmp/test"
    }

    assert {:error, :not_cluster} = Toast.Deployment.server_by_cluster_id(deployment, "PRMR-abc")
  end
end
