defmodule Toast.Client.Collection do
  alias Toast.Client

  @spec create(Client.t(), String.t(), keyword()) :: {:ok, map()} | {:error, term()}
  def create(%Client{} = client, name, opts \\ []) do
    type = if Keyword.get(opts, :edge, false), do: 3, else: 2
    body = %{"name" => name, "type" => type}

    case Client.post(client, "/_api/collection", body) do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(%Client{} = client, name) do
    case Client.delete(client, "/_api/collection/#{name}") do
      {:ok, %{status: status}} when status in 200..299 -> :ok
      {:ok, %{status: 404}} -> :ok
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec list(Client.t(), keyword()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client, opts \\ []) do
    exclude_system = Keyword.get(opts, :exclude_system, false)
    params = if exclude_system, do: [excludeSystem: true], else: []

    case Client.get(client, "/_api/collection", params: params) do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body["result"]}
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end
end
