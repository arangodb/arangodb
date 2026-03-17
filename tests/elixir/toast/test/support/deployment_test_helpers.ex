defmodule Toast.DeploymentTestHelpers do
  @moduledoc false

  def inject_cluster_servers(ctrl, servers, opts \\ []) do
    :sys.replace_state(ctrl, fn state ->
      agents = for {id, s} <- servers, s.role == :agent, do: id
      dbservers = for {id, s} <- servers, s.role == :dbserver, do: id
      coordinators = for {id, s} <- servers, s.role == :coordinator, do: id

      mode_state = %{
        state.mode_state
        | agents: agents,
          dbservers: dbservers,
          coordinators: coordinators
      }

      updated = %{state | servers: servers, mode_state: mode_state}

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
      controller: pid
    }
  end
end
