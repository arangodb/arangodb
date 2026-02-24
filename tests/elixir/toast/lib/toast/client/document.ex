defmodule Toast.Client.Document do
  alias Toast.Client

  @spec insert(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def insert(%Client{} = client, collection, doc) do
    case Client.post(client, "/_api/document/#{collection}", doc) do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec get(Client.t(), String.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, collection, key) do
    case Client.get(client, "/_api/document/#{collection}/#{key}") do
      {:ok, %{status: status, body: body}} when status in 200..299 -> {:ok, body}
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end

  @spec remove(Client.t(), String.t(), String.t()) :: :ok | {:error, term()}
  def remove(%Client{} = client, collection, key) do
    case Client.delete(client, "/_api/document/#{collection}/#{key}") do
      {:ok, %{status: status}} when status in 200..299 -> :ok
      {:ok, resp} -> {:error, %{status: resp.status, body: resp.body}}
      {:error, reason} -> {:error, reason}
    end
  end
end
