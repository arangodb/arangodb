defmodule Toast.Client.Document do
  @moduledoc "Document CRUD operations for ArangoDB collections."

  alias Toast.Client

  @spec insert(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def insert(%Client{} = client, collection, doc) do
    client |> Client.post("/_api/document/#{collection}", doc) |> Client.unwrap()
  end

  @spec get(Client.t(), String.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, collection, key) do
    client |> Client.get("/_api/document/#{collection}/#{key}") |> Client.unwrap()
  end

  @spec remove(Client.t(), String.t(), String.t()) :: :ok | {:error, term()}
  def remove(%Client{} = client, collection, key) do
    client |> Client.delete("/_api/document/#{collection}/#{key}") |> Client.unwrap_ok()
  end
end
