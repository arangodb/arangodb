defmodule Toast.Client.Index do
  @moduledoc "Index management operations for ArangoDB collections."

  alias Toast.Client

  @spec create(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def create(%Client{} = client, collection, definition) do
    client
    |> Client.post("/_api/index", definition, params: [collection: collection])
    |> Client.unwrap()
  end

  @spec list(Client.t(), String.t()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client, collection) do
    with {:ok, body} <-
           client
           |> Client.get("/_api/index", params: [collection: collection])
           |> Client.unwrap() do
      {:ok, body["indexes"]}
    end
  end

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(%Client{} = client, handle) do
    client |> Client.delete("/_api/index/#{handle}") |> Client.unwrap_ok()
  end
end
