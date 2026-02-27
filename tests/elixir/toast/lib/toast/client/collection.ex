defmodule Toast.Client.Collection do
  @moduledoc "Collection management operations for ArangoDB."

  alias Toast.Client

  @spec create(Client.t(), String.t(), keyword()) :: {:ok, map()} | {:error, term()}
  def create(%Client{} = client, name, opts \\ []) do
    type = if Keyword.get(opts, :edge, false), do: 3, else: 2
    body = %{"name" => name, "type" => type}
    client |> Client.post("/_api/collection", body) |> Client.unwrap()
  end

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(%Client{} = client, name) do
    case Client.delete(client, "/_api/collection/#{name}") do
      {:ok, %{status: 404}} -> :ok
      other -> Client.unwrap_ok(other)
    end
  end

  @spec list(Client.t(), keyword()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client, opts \\ []) do
    exclude_system = Keyword.get(opts, :exclude_system, false)
    params = if exclude_system, do: [excludeSystem: true], else: []

    with {:ok, body} <-
           client |> Client.get("/_api/collection", params: params) |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end
end
