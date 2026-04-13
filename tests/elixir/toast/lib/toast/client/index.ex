defmodule Toast.Client.Index do
  @moduledoc """
  Index management operations for ArangoDB collections.

      Index.ensure!(client, "users", :persistent, ["email"], unique: true)
      Index.ensure!(client, "events", :ttl, ["createdAt"], expire_after: 3600)
      {:ok, indexes} = Index.list(client, "users", with_stats: true)
      Index.drop!(client, idx["id"])
  """

  alias Toast.Client
  require Client
  import Toast.Client.Utils, only: [translate_opts: 2]

  @ensure_body_keys %{
    unique: "unique",
    sparse: "sparse",
    deduplicate: "deduplicate",
    estimates: "estimates",
    cache_enabled: "cacheEnabled",
    in_background: "inBackground",
    name: "name",
    expire_after: "expireAfter",
    geo_json: "geoJson",
    legacy_polygons: "legacyPolygons",
    stored_values: "storedValues",
    field_value_types: "fieldValueTypes",
    prefix_fields: "prefixFields"
  }

  @list_param_keys %{with_stats: :withStats, with_hidden: :withHidden}

  @doc """
  Creates an index on `collection` if it doesn't already exist.

  ## Options

    * `:unique` — create a unique index
    * `:sparse` — create a sparse index (don't index docs missing the field)
    * `:deduplicate` — deduplicate array values in unique indexes (default `true`)
    * `:estimates` — maintain selectivity estimates (default `true`)
    * `:cache_enabled` — enable in-memory hash cache for equality lookups
    * `:in_background` — create index without exclusive lock
    * `:name` — optional index name
    * `:expire_after` — TTL in seconds (for `:ttl` indexes)
    * `:geo_json` — interpret fields as GeoJSON (for `:geo` indexes)
    * `:legacy_polygons` — use legacy polygon handling (for `:geo` indexes)
    * `:stored_values` — additional attributes to store in the index
    * `:field_value_types` — field value types (for `:mdi` indexes)
    * `:prefix_fields` — prefix fields (for `:mdi_prefixed` indexes)
  """
  @spec ensure(Client.t(), String.t(), atom(), [String.t()], keyword()) ::
          {:ok, map()} | {:error, term()}
  def ensure(%Client{} = client, collection, type, fields, opts \\ []) do
    extra = translate_opts(opts, @ensure_body_keys) |> Map.new()
    body = Map.merge(%{"type" => to_string(type), "fields" => fields}, extra)

    client
    |> Client.post("/_api/index", body, params: [collection: collection])
    |> Client.unwrap()
  end

  @doc """
  Lists indexes on `collection`.

  ## Options

    * `:with_stats` — include index statistics
    * `:with_hidden` — include hidden indexes
  """
  @spec list(Client.t(), String.t(), keyword()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client, collection, opts \\ []) do
    params = [collection: collection] ++ translate_opts(opts, @list_param_keys)

    with {:ok, body} <- client |> Client.get("/_api/index", params: params) |> Client.unwrap() do
      {:ok, body["indexes"]}
    end
  end

  @spec get(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, handle) do
    client |> Client.get("/_api/index/#{handle}") |> Client.unwrap()
  end

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(%Client{} = client, handle) do
    Client.idempotent_delete(client, "/_api/index/#{handle}")
  end

  def ensure!(%Client{} = client, collection, type, fields, opts \\ []),
    do: Client.bang!(ensure(client, collection, type, fields, opts))

  def list!(%Client{} = client, collection, opts \\ []),
    do: Client.bang!(list(client, collection, opts))

  def get!(%Client{} = client, handle), do: Client.bang!(get(client, handle))
  def drop!(%Client{} = client, handle), do: Client.bang!(drop(client, handle))
end
