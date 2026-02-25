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

  @spec set(Deployment.t(), String.t(), String.t()) :: :ok | {:error, term()}
  def set(%Deployment{} = deployment, target, name) when is_binary(target) do
    with {:ok, client} <- Deployment.client(deployment, target) do
      do_set(client, name)
    end
  end

  @spec clear(Deployment.t(), String.t(), String.t()) :: :ok | {:error, term()}
  def clear(%Deployment{} = deployment, target, name) when is_binary(target) do
    with {:ok, client} <- Deployment.client(deployment, target) do
      do_clear(client, name)
    end
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

  # -- Response handling --

  defp handle_response({:ok, %{status: 200}}), do: :ok
  defp handle_response({:ok, %{status: 404}}), do: {:error, :not_supported}
  defp handle_response({:ok, %{status: status}}), do: {:error, {:unexpected_status, status}}
  defp handle_response({:error, _} = error), do: error
end
