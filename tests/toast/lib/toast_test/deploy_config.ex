defmodule ToastTest.DeployConfig do
  @moduledoc false

  @doc """
  Build a `Toast.Deployment.Config` from flat suite-style options.

  Translates the user-facing vocabulary (`:mode`, `:server_args`,
  `:authentication`, `:cluster_dbservers`, etc.) into the struct fields
  that `Toast.Deployment.Config.new/1` expects.

  The `mode` argument must already be resolved (`:single_server` or `:cluster`).
  """
  @spec build(:single_server | :cluster, keyword()) :: Toast.Deployment.Config.t()
  def build(mode, opts) do
    server_args_override =
      case Keyword.get(opts, :server_args, %{}) do
        args when args != %{} -> [server_args: args]
        _ -> []
      end

    cluster_override = cluster_override_opts(mode, opts)
    auth_override = auth_override_opts(opts)

    Toast.Deployment.Config.new(server_args_override ++ cluster_override ++ auth_override)
  end

  defp auth_override_opts(opts) do
    case Keyword.get(opts, :authentication, false) do
      true ->
        [authentication: true, jwt_algorithm: Keyword.get(opts, :jwt_algorithm, :hmac)]

      _ ->
        []
    end
  end

  defp cluster_override_opts(:single_server, _opts), do: []

  defp cluster_override_opts(:cluster, opts) do
    topology =
      for {suite_key, cluster_key} <- [
            cluster_agents: :agents,
            cluster_dbservers: :dbservers,
            cluster_coordinators: :coordinators,
            replication_factor: :replication_factor
          ],
          val = Keyword.get(opts, suite_key),
          val != nil,
          do: {cluster_key, val}

    role_args =
      for key <- [:coordinator_args, :dbserver_args, :agent_args],
          args = Keyword.get(opts, key, %{}),
          args != %{},
          do: {key, args}

    opts = topology ++ role_args
    [cluster: if(opts == [], do: true, else: opts)]
  end
end
