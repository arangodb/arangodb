defmodule Toast.DeploymentTestHelpers do
  @moduledoc false

  def inject_cluster_servers(ctrl, servers, opts \\ []) do
    :sys.replace_state(ctrl, fn state ->
      updated = %{state | servers: servers}

      case Keyword.get(opts, :status) do
        nil -> updated
        status -> %{updated | status: status}
      end
    end)
  end

  def make_deployment(pid, opts \\ []) do
    %Toast.Deployment{
      id: Keyword.get(opts, :id, "test-deployment"),
      controller: pid
    }
  end
end
