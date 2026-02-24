defmodule Toast.Client.Index do
  alias Toast.Client

  @spec create(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def create(%Client{} = client, collection, definition) do
    case Client.post(client, "/_api/index", definition, params: [collection: collection]) do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec list(Client.t(), String.t()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client, collection) do
    case Client.get(client, "/_api/index", params: [collection: collection]) do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body["indexes"]}
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(%Client{} = client, handle) do
    case Client.delete(client, "/_api/index/#{handle}") do
      {:ok, %{status: status}} when status in 200..299 -> :ok
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end
end
