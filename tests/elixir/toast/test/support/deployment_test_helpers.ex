defmodule Toast.DeploymentTestHelpers do
  @moduledoc false

  def inject_cluster_servers(ctrl, servers, opts \\ []) do
    :sys.replace_state(ctrl, fn state ->
      agents = for {id, s} <- servers, s.role == :agent, do: id
      dbservers = for {id, s} <- servers, s.role == :dbserver, do: id
      coordinators = for {id, s} <- servers, s.role == :coordinator, do: id

      updated = %{state |
        servers: servers,
        agents: agents,
        dbservers: dbservers,
        coordinators: coordinators
      }

      case Keyword.get(opts, :status) do
        nil -> updated
        status -> %{updated | status: status}
      end
    end)
  end

  def make_deployment(pid, opts \\ []) do
    %Toast.Deployment{
      id: Keyword.get(opts, :id, "test-deployment"),
      mode: Keyword.get(opts, :mode, :single_server),
      config: Keyword.get(opts, :config, Toast.Config.load()),
      endpoint: "http://127.0.0.1:0",
      controller: pid,
      work_dir: "/tmp/toast-test"
    }
  end
end
