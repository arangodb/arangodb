defmodule Toast.Deployment.FailurePoint do
  @moduledoc "Failure point management for ArangoDB debug builds."

  alias Toast.Client
  alias Toast.Deployment

  @base_path "/_admin/debug/failat"

  # -- Low-level (client-based) --

  @spec do_set(Client.t(), String.t()) :: :ok | {:error, term()}
  def do_set(%Client{} = client, name) do
    client
    |> Client.put("#{@base_path}/#{name}")
    |> handle_response()
  end

  @spec do_clear(Client.t(), String.t()) :: :ok | {:error, term()}
  def do_clear(%Client{} = client, name) do
    client
    |> Client.delete("#{@base_path}/#{name}")
    |> handle_response()
  end

  @spec do_clear_all(Client.t()) :: :ok | {:error, term()}
  def do_clear_all(%Client{} = client) do
    client
    |> Client.delete(@base_path)
    |> handle_response()
  end

  # -- High-level (deployment-based) --

  @spec set(Deployment.t(), Deployment.server_target(), String.t()) :: :ok | {:error, term()}
  def set(%Deployment{} = deployment, target, name) when is_binary(target) do
    with {:ok, client} <- Deployment.client(deployment, target) do
      do_set(client, name)
    end
  end

  def set(%Deployment{} = deployment, target, name) when is_list(target) do
    apply_to_matching_servers(deployment, target, &do_set(&1, name))
  end

  @spec clear(Deployment.t(), Deployment.server_target(), String.t()) :: :ok | {:error, term()}
  def clear(%Deployment{} = deployment, target, name) when is_binary(target) do
    with {:ok, client} <- Deployment.client(deployment, target) do
      do_clear(client, name)
    end
  end

  def clear(%Deployment{} = deployment, target, name) when is_list(target) do
    apply_to_matching_servers(deployment, target, &do_clear(&1, name))
  end

  @spec clear_all(Deployment.t()) :: :ok | {:error, term()}
  def clear_all(%Deployment{} = deployment) do
    results =
      deployment
      |> Deployment.servers()
      |> Enum.map(fn server ->
        server.endpoint
        |> Client.new()
        |> do_clear_all()
      end)

    case Enum.find(results, &match?({:error, _}, &1)) do
      nil -> :ok
      error -> error
    end
  end

  defp apply_to_matching_servers(deployment, target, fun) do
    with {:ok, clients} <- resolve_target_clients(deployment, target) do
      results = Enum.map(clients, fun)

      case Enum.find(results, &match?({:error, _}, &1)) do
        nil -> :ok
        error -> error
      end
    end
  end

  defp resolve_target_clients(deployment, role: role) do
    case Deployment.servers(deployment, role: role) do
      [] -> {:error, {:no_servers_for_role, role}}
      servers -> {:ok, Enum.map(servers, &Client.new(&1.endpoint))}
    end
  end

  defp resolve_target_clients(deployment, [role: _role, index: _index] = target) do
    with {:ok, client} <- Deployment.client(deployment, target) do
      {:ok, [client]}
    end
  end

  defp resolve_target_clients(deployment, cluster_id: cluster_internal_id) do
    case Deployment.server_by_cluster_id(deployment, cluster_internal_id) do
      {:ok, server} -> {:ok, [Client.new(server.endpoint)]}
      {:error, _} = err -> err
    end
  end

  defp resolve_target_clients(_deployment, target) do
    {:error, {:invalid_target, target}}
  end

  # -- Response handling --

  defp handle_response({:ok, %{status: 200}}), do: :ok
  defp handle_response({:ok, %{status: 404}}), do: {:error, :not_supported}
  defp handle_response({:ok, %{status: status}}), do: {:error, {:unexpected_status, status}}
  defp handle_response({:error, _} = error), do: error
end
