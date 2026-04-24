defmodule Toast.Client.Graph do
  @moduledoc """
  Named graph lifecycle and structure management via the Gharial API.

      # create a graph with an edge definition
      edge_def = %{collection: "edges", from: ["startV"], to: ["endV"]}
      graph = Graph.create!(client, "my_graph", [edge_def])

      # add an orphan vertex collection
      Graph.add_vertex_collection!(client, "my_graph", "orphans")

      # tear down (dropping all associated collections)
      Graph.drop!(client, "my_graph", drop_collections: true)

  All non-bang functions return `{:ok, result}` or `{:error, reason}`.
  Bang variants raise on error.
  """

  alias Toast.Client
  require Client
  import Toast.Client.Utils, only: [translate_opts: 2]

  @body_keys %{orphan_collections: "orphanCollections", is_smart: "isSmart"}
  @drop_param_keys %{drop_collections: :dropCollections}
  @remove_vertex_param_keys %{drop_collection: :dropCollection}

  @spec list(Client.t()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client) do
    with {:ok, body} <- client |> Client.get("/_api/gharial") |> Client.unwrap() do
      {:ok, body["graphs"]}
    end
  end

  @doc """
  Creates a named graph with the given edge definitions.

  ## Options

    * `:orphan_collections` - list of additional vertex collections not referenced by any edge definition
    * `:is_smart` - whether to create a SmartGraph (Enterprise only)
    * `:options` - map of additional graph creation options (e.g., `%{numberOfShards: 3}`)
  """
  @spec create(Client.t(), String.t(), [map()], keyword()) :: {:ok, map()} | {:error, term()}
  def create(%Client{} = client, name, edge_definitions, opts \\ []) do
    body = Map.merge(to_body(opts), %{"name" => name, "edgeDefinitions" => edge_definitions})

    with {:ok, body} <- client |> Client.post("/_api/gharial", body) |> Client.unwrap() do
      {:ok, body["graph"]}
    end
  end

  @spec get(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, name) do
    with {:ok, body} <- client |> Client.get("/_api/gharial/#{name}") |> Client.unwrap() do
      {:ok, body["graph"]}
    end
  end

  @doc """
  Drops a named graph. Returns `:ok` even if the graph does not exist.

  ## Options

    * `:drop_collections` - if `true`, also drops all collections owned by the graph
  """
  @spec drop(Client.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def drop(%Client{} = client, name, opts \\ []) do
    Client.idempotent_delete(client, "/_api/gharial/#{name}",
      params: translate_opts(opts, @drop_param_keys)
    )
  end

  @spec add_vertex_collection(Client.t(), String.t(), String.t()) ::
          {:ok, map()} | {:error, term()}
  def add_vertex_collection(%Client{} = client, graph, collection) do
    body = %{"collection" => collection}

    with {:ok, body} <-
           client |> Client.post("/_api/gharial/#{graph}/vertex", body) |> Client.unwrap() do
      {:ok, body["graph"]}
    end
  end

  @doc """
  Removes a vertex collection from the graph.

  ## Options

    * `:drop_collection` - if `true`, also drops the collection itself
  """
  @spec remove_vertex_collection(Client.t(), String.t(), String.t(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def remove_vertex_collection(%Client{} = client, graph, collection, opts \\ []) do
    with {:ok, body} <-
           client
           |> Client.delete("/_api/gharial/#{graph}/vertex/#{collection}",
             params: translate_opts(opts, @remove_vertex_param_keys)
           )
           |> Client.unwrap() do
      {:ok, body["graph"]}
    end
  end

  @spec add_edge_definition(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def add_edge_definition(%Client{} = client, graph, definition) do
    with {:ok, body} <-
           client |> Client.post("/_api/gharial/#{graph}/edge", definition) |> Client.unwrap() do
      {:ok, body["graph"]}
    end
  end

  @spec replace_edge_definition(Client.t(), String.t(), String.t(), map()) ::
          {:ok, map()} | {:error, term()}
  def replace_edge_definition(%Client{} = client, graph, collection, definition) do
    with {:ok, body} <-
           client
           |> Client.put("/_api/gharial/#{graph}/edge/#{collection}", definition)
           |> Client.unwrap() do
      {:ok, body["graph"]}
    end
  end

  @doc """
  Removes an edge definition from the graph.

  ## Options

    * `:drop_collections` - if `true`, also drops the edge collection and orphaned vertex collections
  """
  @spec remove_edge_definition(Client.t(), String.t(), String.t(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def remove_edge_definition(%Client{} = client, graph, collection, opts \\ []) do
    with {:ok, body} <-
           client
           |> Client.delete("/_api/gharial/#{graph}/edge/#{collection}",
             params: translate_opts(opts, @drop_param_keys)
           )
           |> Client.unwrap() do
      {:ok, body["graph"]}
    end
  end

  def list!(%Client{} = client), do: Client.bang!(list(client))

  def create!(%Client{} = client, name, edge_definitions, opts \\ []),
    do: Client.bang!(create(client, name, edge_definitions, opts))

  def get!(%Client{} = client, name), do: Client.bang!(get(client, name))
  def drop!(%Client{} = client, name, opts \\ []), do: Client.bang!(drop(client, name, opts))

  def add_vertex_collection!(%Client{} = client, graph, collection),
    do: Client.bang!(add_vertex_collection(client, graph, collection))

  def remove_vertex_collection!(%Client{} = client, graph, collection, opts \\ []),
    do: Client.bang!(remove_vertex_collection(client, graph, collection, opts))

  def add_edge_definition!(%Client{} = client, graph, definition),
    do: Client.bang!(add_edge_definition(client, graph, definition))

  def replace_edge_definition!(%Client{} = client, graph, collection, definition),
    do: Client.bang!(replace_edge_definition(client, graph, collection, definition))

  def remove_edge_definition!(%Client{} = client, graph, collection, opts \\ []),
    do: Client.bang!(remove_edge_definition(client, graph, collection, opts))

  defp to_body(opts) do
    {options, opts} = Keyword.pop(opts, :options)
    base = translate_opts(opts, @body_keys) |> Map.new()
    if options, do: Map.put(base, "options", options), else: base
  end
end
