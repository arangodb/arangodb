################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule Toast.Client.Edge do
  @moduledoc """
  CRUD operations for edges within a named graph (Gharial API).

      doc = %{_from: "startV/a", _to: "endV/b", weight: 1.5}
      edge = Edge.insert!(client, "my_graph", "edges", doc, return_new: true)
      Edge.update!(client, "my_graph", "edges", edge["_key"], %{weight: 2.0})
      Edge.remove!(client, "my_graph", "edges", edge["_key"])

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
  Inserts an edge into a graph-managed edge collection. The document must
  contain `_from` and `_to` attributes.

  ## Options

    * `:wait_for_sync` - wait for the document to be synced to disk
    * `:return_new` - include the new document in the response
  """
  @spec insert(Client.t(), String.t(), String.t(), map(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def insert(%Client{} = client, graph, collection, doc, opts \\ []) do
    with {:ok, body} <-
           client
           |> Client.post("/_api/gharial/#{graph}/edge/#{collection}", doc,
             params: to_params(opts)
           )
           |> Client.unwrap() do
      {:ok, body["edge"]}
    end
  end

  @spec get(Client.t(), String.t(), String.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, graph, collection, key) do
    with {:ok, body} <-
           client
           |> Client.get("/_api/gharial/#{graph}/edge/#{collection}/#{key}")
           |> Client.unwrap() do
      {:ok, body["edge"]}
    end
  end

  @doc """
  Partially updates an edge (merge-patch).

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
           |> Client.patch("/_api/gharial/#{graph}/edge/#{collection}/#{key}", patch,
             params: to_params(opts)
           )
           |> Client.unwrap() do
      {:ok, body["edge"]}
    end
  end

  @doc """
  Replaces an edge document entirely.

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
           |> Client.put("/_api/gharial/#{graph}/edge/#{collection}/#{key}", doc,
             params: to_params(opts)
           )
           |> Client.unwrap() do
      {:ok, body["edge"]}
    end
  end

  @doc """
  Removes an edge from the graph.

  ## Options

    * `:wait_for_sync` - wait for the removal to be synced to disk
  """
  @spec remove(Client.t(), String.t(), String.t(), String.t(), keyword()) ::
          :ok | {:error, term()}
  def remove(%Client{} = client, graph, collection, key, opts \\ []) do
    client
    |> Client.delete("/_api/gharial/#{graph}/edge/#{collection}/#{key}",
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
