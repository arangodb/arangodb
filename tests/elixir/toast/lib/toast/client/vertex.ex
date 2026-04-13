defmodule Toast.Client.Vertex do
  @moduledoc """
  CRUD operations for vertices within a named graph (Gharial API).

      vtx = Vertex.insert!(client, "my_graph", "vertices", %{name: "Alice"})
      Vertex.update!(client, "my_graph", "vertices", vtx["_key"], %{age: 30})
      Vertex.remove!(client, "my_graph", "vertices", vtx["_key"])

  All non-bang functions return `{:ok, result}` or `{:error, reason}`.
  Bang variants raise on error.
  """

  alias Toast.Client
  require Client
  import Toast.Client.Utils, only: [translate_opts: 2]

  @param_keys %{
    wait_for_sync: :waitForSync,
    return_new: :returnNew,
    return_old: :returnOld,
    keep_null: :keepNull
  }

  @doc """
  Inserts a vertex into a graph-managed vertex collection.

  ## Options

    * `:wait_for_sync` - wait for the document to be synced to disk
    * `:return_new` - include the new document in the response
  """
  @spec insert(Client.t(), String.t(), String.t(), map(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def insert(%Client{} = client, graph, collection, doc, opts \\ []) do
    with {:ok, body} <-
           client
           |> Client.post("/_api/gharial/#{graph}/vertex/#{collection}", doc,
             params: to_params(opts)
           )
           |> Client.unwrap() do
      {:ok, body["vertex"]}
    end
  end

  @spec get(Client.t(), String.t(), String.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, graph, collection, key) do
    with {:ok, body} <-
           client
           |> Client.get("/_api/gharial/#{graph}/vertex/#{collection}/#{key}")
           |> Client.unwrap() do
      {:ok, body["vertex"]}
    end
  end

  @doc """
  Partially updates a vertex (merge-patch).

  ## Options

    * `:wait_for_sync` - wait for the update to be synced to disk
    * `:return_new` - include the updated document in the response
    * `:return_old` - include the previous document in the response
    * `:keep_null` - if `false`, removes attributes set to `nil` instead of storing them
  """
  @spec update(Client.t(), String.t(), String.t(), String.t(), map(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def update(%Client{} = client, graph, collection, key, patch, opts \\ []) do
    with {:ok, body} <-
           client
           |> Client.patch("/_api/gharial/#{graph}/vertex/#{collection}/#{key}", patch,
             params: to_params(opts)
           )
           |> Client.unwrap() do
      {:ok, body["vertex"]}
    end
  end

  @doc """
  Replaces a vertex document entirely.

  ## Options

    * `:wait_for_sync` - wait for the operation to be synced to disk
    * `:return_new` - include the new document in the response
    * `:return_old` - include the previous document in the response
  """
  @spec replace(Client.t(), String.t(), String.t(), String.t(), map(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def replace(%Client{} = client, graph, collection, key, doc, opts \\ []) do
    with {:ok, body} <-
           client
           |> Client.put("/_api/gharial/#{graph}/vertex/#{collection}/#{key}", doc,
             params: to_params(opts)
           )
           |> Client.unwrap() do
      {:ok, body["vertex"]}
    end
  end

  @doc """
  Removes a vertex from the graph.

  ## Options

    * `:wait_for_sync` - wait for the removal to be synced to disk
  """
  @spec remove(Client.t(), String.t(), String.t(), String.t(), keyword()) ::
          :ok | {:error, term()}
  def remove(%Client{} = client, graph, collection, key, opts \\ []) do
    client
    |> Client.delete("/_api/gharial/#{graph}/vertex/#{collection}/#{key}",
      params: to_params(opts)
    )
    |> Client.unwrap_ok()
  end

  def insert!(%Client{} = client, graph, collection, doc, opts \\ []),
    do: Client.bang!(insert(client, graph, collection, doc, opts))

  def get!(%Client{} = client, graph, collection, key),
    do: Client.bang!(get(client, graph, collection, key))

  def update!(%Client{} = client, graph, collection, key, patch, opts \\ []),
    do: Client.bang!(update(client, graph, collection, key, patch, opts))

  def replace!(%Client{} = client, graph, collection, key, doc, opts \\ []),
    do: Client.bang!(replace(client, graph, collection, key, doc, opts))

  def remove!(%Client{} = client, graph, collection, key, opts \\ []),
    do: Client.bang!(remove(client, graph, collection, key, opts))

  defp to_params(opts), do: translate_opts(opts, @param_keys)
end
