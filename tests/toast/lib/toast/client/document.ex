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

defmodule Toast.Client.Document do
  @moduledoc """
  Document CRUD operations for ArangoDB collections.

  All write operations accept common opts documented on each function.
  Functions with `!` suffixes raise on error instead of returning `{:error, reason}`.

      # Insert and read back
      {:ok, meta} = Document.insert(client, "users", %{name: "Alice"}, return_new: true)
      {:ok, doc} = Document.get(client, "users", meta["_key"])

      # Conditional update with revision check
      Document.update!(client, "users", key, %{age: 30},
        headers: [if_match: doc["_rev"]], return_new: true)

      # Bulk insert
      {:ok, results} = Document.insert_many(client, "users", [%{name: "Bob"}, %{name: "Carol"}])
  """

  alias Toast.Client
  require Client
  import Toast.Client.Utils, only: [translate_opts: 2]

  @param_keys %{
    wait_for_sync: :waitForSync,
    return_new: :returnNew,
    return_old: :returnOld,
    silent: :silent,
    overwrite_mode: :overwriteMode,
    keep_null: :keepNull,
    merge_objects: :mergeObjects,
    ignore_revs: :ignoreRevs
  }

  @header_keys %{if_match: "If-Match", if_none_match: "If-None-Match"}

  @doc """
  Inserts a document into `collection`.

  ## Options

    * `:wait_for_sync` — wait for fsync before returning
    * `:return_new` — include the new document in the response
    * `:silent` — omit the response body
    * `:overwrite_mode` — `:conflict` (default), `:replace`, `:update`, or `:ignore`
    * `:keep_null` — whether to store `null` values (relevant with `:overwrite_mode`)
    * `:merge_objects` — merge objects on update (relevant with `:overwrite_mode`)
  """
  @spec insert(Client.t(), String.t(), map(), keyword()) :: {:ok, map()} | {:error, term()}
  def insert(%Client{} = client, collection, doc, opts \\ []) do
    client
    |> Client.post("/_api/document/#{collection}", doc, params: to_params(opts))
    |> Client.unwrap()
  end

  @doc """
  Fetches a document by key.

  ## Options

    * `:headers` — conditional request headers:
      * `:if_match` — only return if revision matches (ETag)
      * `:if_none_match` — only return if revision does not match
  """
  @spec get(Client.t(), String.t(), String.t(), keyword()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, collection, key, opts \\ []) do
    {headers, _opts} = pop_headers(opts)

    client
    |> Client.get("/_api/document/#{collection}/#{key}", headers: headers)
    |> Client.unwrap()
  end

  @doc """
  Partially updates a document by key.

  ## Options

    * `:wait_for_sync`, `:return_new`, `:return_old`, `:silent` — see `insert/4`
    * `:keep_null` — whether to persist `null` values (default `true`)
    * `:merge_objects` — deep-merge nested objects (default `true`)
    * `:ignore_revs` — skip revision check (default `true`)
    * `:headers` — conditional request headers (see `get/4`)
  """
  @spec update(Client.t(), String.t(), String.t(), map(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def update(%Client{} = client, collection, key, patch, opts \\ []) do
    {headers, opts} = pop_headers(opts)

    client
    |> Client.patch("/_api/document/#{collection}/#{key}", patch,
      params: to_params(opts),
      headers: headers
    )
    |> Client.unwrap()
  end

  @doc """
  Replaces a document by key.

  ## Options

    * `:wait_for_sync`, `:return_new`, `:return_old`, `:silent` — see `insert/4`
    * `:ignore_revs` — skip revision check (default `true`)
    * `:headers` — conditional request headers (see `get/4`)
  """
  @spec replace(Client.t(), String.t(), String.t(), map(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def replace(%Client{} = client, collection, key, doc, opts \\ []) do
    {headers, opts} = pop_headers(opts)

    client
    |> Client.put("/_api/document/#{collection}/#{key}", doc,
      params: to_params(opts),
      headers: headers
    )
    |> Client.unwrap()
  end

  @doc """
  Removes a document by key.

  ## Options

    * `:wait_for_sync`, `:return_old`, `:silent` — see `insert/4`
    * `:ignore_revs` — skip revision check (default `true`)
    * `:headers` — conditional request headers (see `get/4`)
  """
  @spec remove(Client.t(), String.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def remove(%Client{} = client, collection, key, opts \\ []) do
    {headers, opts} = pop_headers(opts)

    client
    |> Client.delete("/_api/document/#{collection}/#{key}",
      params: to_params(opts),
      headers: headers
    )
    |> Client.unwrap_ok()
  end

  @doc """
  Inserts multiple documents. Options same as `insert/4`.
  """
  @spec insert_many(Client.t(), String.t(), [map()], keyword()) ::
          {:ok, [map()]} | {:error, term()}
  def insert_many(%Client{} = client, collection, docs, opts \\ []) do
    client
    |> Client.post("/_api/document/#{collection}", docs, params: to_params(opts))
    |> Client.unwrap()
  end

  @doc """
  Partially updates multiple documents. Options same as `update/5` (except `:headers`).
  """
  @spec update_many(Client.t(), String.t(), [map()], keyword()) ::
          {:ok, [map()]} | {:error, term()}
  def update_many(%Client{} = client, collection, patches, opts \\ []) do
    client
    |> Client.patch("/_api/document/#{collection}", patches, params: to_params(opts))
    |> Client.unwrap()
  end

  @doc """
  Replaces multiple documents. Options same as `replace/5` (except `:headers`).
  """
  @spec replace_many(Client.t(), String.t(), [map()], keyword()) ::
          {:ok, [map()]} | {:error, term()}
  def replace_many(%Client{} = client, collection, docs, opts \\ []) do
    client
    |> Client.put("/_api/document/#{collection}", docs, params: to_params(opts))
    |> Client.unwrap()
  end

  @doc """
  Removes multiple documents. Options same as `remove/4` (except `:headers`).
  """
  @spec remove_many(Client.t(), String.t(), [map() | String.t()], keyword()) ::
          {:ok, [map()]} | {:error, term()}
  def remove_many(%Client{} = client, collection, keys, opts \\ []) do
    client
    |> Client.delete("/_api/document/#{collection}", params: to_params(opts), body: keys)
    |> Client.unwrap()
  end

  def insert!(%Client{} = client, collection, doc, opts \\ []),
    do: Client.bang!(insert(client, collection, doc, opts))

  def get!(%Client{} = client, collection, key, opts \\ []),
    do: Client.bang!(get(client, collection, key, opts))

  def update!(%Client{} = client, collection, key, patch, opts \\ []),
    do: Client.bang!(update(client, collection, key, patch, opts))

  def replace!(%Client{} = client, collection, key, doc, opts \\ []),
    do: Client.bang!(replace(client, collection, key, doc, opts))

  def remove!(%Client{} = client, collection, key, opts \\ []),
    do: Client.bang!(remove(client, collection, key, opts))

  def insert_many!(%Client{} = client, collection, docs, opts \\ []),
    do: Client.bang!(insert_many(client, collection, docs, opts))

  def update_many!(%Client{} = client, collection, patches, opts \\ []),
    do: Client.bang!(update_many(client, collection, patches, opts))

  def replace_many!(%Client{} = client, collection, docs, opts \\ []),
    do: Client.bang!(replace_many(client, collection, docs, opts))

  def remove_many!(%Client{} = client, collection, keys, opts \\ []),
    do: Client.bang!(remove_many(client, collection, keys, opts))

  defp to_params(opts), do: translate_opts(opts, @param_keys)

  defp pop_headers(opts) do
    {header_opts, rest} = Keyword.pop(opts, :headers, [])
    {translate_opts(header_opts, @header_keys), rest}
  end
end
