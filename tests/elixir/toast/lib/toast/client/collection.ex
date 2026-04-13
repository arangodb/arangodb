defmodule Toast.Client.Collection do
  @moduledoc """
  Collection management operations for ArangoDB.

      Collection.create!(client, "users", number_of_shards: 3, replication_factor: 2)
      Collection.create_edge!(client, "friends")
      Collection.truncate!(client, "users")
      Collection.drop!(client, "users")
  """

  alias Toast.Client
  require Client
  import Toast.Client.Utils, only: [translate_opts: 2]

  @create_body_keys %{
    wait_for_sync: "waitForSync",
    is_system: "isSystem",
    number_of_shards: "numberOfShards",
    replication_factor: "replicationFactor",
    write_concern: "writeConcern",
    shard_keys: "shardKeys",
    distribute_shards_like: "distributeShardsLike",
    sharding_strategy: "shardingStrategy",
    cache_enabled: "cacheEnabled",
    schema: "schema",
    computed_values: "computedValues",
    key_options: "keyOptions"
  }

  @truncate_param_keys %{
    wait_for_sync: :waitForSync,
    compact: :compact
  }

  @doc """
  Creates a document collection.

  ## Options

    * `:wait_for_sync` — wait for data to be synced to disk
    * `:is_system` — create a system collection
    * `:number_of_shards` — number of shards (cluster only)
    * `:replication_factor` — number of copies per shard (cluster only)
    * `:write_concern` — minimum in-sync copies before writes are accepted (cluster only)
    * `:shard_keys` — list of attributes to determine target shard (cluster only)
    * `:distribute_shards_like` — name of prototype collection for shard distribution
    * `:sharding_strategy` — sharding algorithm (e.g. `"hash"`)
    * `:cache_enabled` — enable in-memory document cache
    * `:schema` — collection-level JSON schema validation config (map)
    * `:computed_values` — list of computed value definitions (list of maps)
    * `:key_options` — key generation options (map)
  """
  @spec create(Client.t(), String.t(), keyword()) :: {:ok, map()} | {:error, term()}
  def create(%Client{} = client, name, opts \\ []) do
    do_create(client, name, 2, opts)
  end

  @doc """
  Creates an edge collection. Accepts the same options as `create/3`.
  """
  @spec create_edge(Client.t(), String.t(), keyword()) :: {:ok, map()} | {:error, term()}
  def create_edge(%Client{} = client, name, opts \\ []) do
    do_create(client, name, 3, opts)
  end

  @doc """
  Drops a collection. Returns `:ok` even if the collection does not exist.

  ## Options

    * `:is_system` — allow dropping system collections (default: `false`)
  """
  @spec drop(Client.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def drop(%Client{} = client, name, opts \\ []) do
    params = if Keyword.get(opts, :is_system, false), do: [isSystem: true], else: []
    Client.idempotent_delete(client, "/_api/collection/#{name}", params: params)
  end

  @doc """
  Lists all collections in the current database.

  ## Options

    * `:exclude_system` — exclude system collections (default: `false`)
  """
  @spec list(Client.t(), keyword()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client, opts \\ []) do
    exclude_system = Keyword.get(opts, :exclude_system, false)
    params = if exclude_system, do: [excludeSystem: true], else: []

    with {:ok, body} <-
           client |> Client.get("/_api/collection", params: params) |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @spec info(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def info(%Client{} = client, name) do
    client |> Client.get("/_api/collection/#{name}") |> Client.unwrap()
  end

  @spec properties(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def properties(%Client{} = client, name) do
    client |> Client.get("/_api/collection/#{name}/properties") |> Client.unwrap()
  end

  @spec update_properties(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def update_properties(%Client{} = client, name, props) do
    client |> Client.put("/_api/collection/#{name}/properties", props) |> Client.unwrap()
  end

  @spec count(Client.t(), String.t()) :: {:ok, non_neg_integer()} | {:error, term()}
  def count(%Client{} = client, name) do
    with {:ok, body} <- client |> Client.get("/_api/collection/#{name}/count") |> Client.unwrap() do
      {:ok, body["count"]}
    end
  end

  @doc """
  Removes all documents from a collection.

  ## Options

    * `:wait_for_sync` — wait for data to be synced to disk
    * `:compact` — compact the collection after truncation
  """
  @spec truncate(Client.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def truncate(%Client{} = client, name, opts \\ []) do
    params = translate_opts(opts, @truncate_param_keys)

    client
    |> Client.put("/_api/collection/#{name}/truncate", nil, params: params)
    |> Client.unwrap_ok()
  end

  def create!(%Client{} = client, name, opts \\ []), do: Client.bang!(create(client, name, opts))

  def create_edge!(%Client{} = client, name, opts \\ []),
    do: Client.bang!(create_edge(client, name, opts))

  def drop!(%Client{} = client, name, opts \\ []), do: Client.bang!(drop(client, name, opts))
  def list!(%Client{} = client, opts \\ []), do: Client.bang!(list(client, opts))
  def info!(%Client{} = client, name), do: Client.bang!(info(client, name))
  def properties!(%Client{} = client, name), do: Client.bang!(properties(client, name))

  def update_properties!(%Client{} = client, name, props),
    do: Client.bang!(update_properties(client, name, props))

  def count!(%Client{} = client, name), do: Client.bang!(count(client, name))

  def truncate!(%Client{} = client, name, opts \\ []),
    do: Client.bang!(truncate(client, name, opts))

  defp do_create(client, name, type, opts) do
    extra = translate_opts(opts, @create_body_keys) |> Map.new()
    body = Map.merge(%{"name" => name, "type" => type}, extra)
    client |> Client.post("/_api/collection", body) |> Client.unwrap()
  end
end
