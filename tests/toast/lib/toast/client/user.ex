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

defmodule Toast.Client.User do
  @moduledoc """
  User management operations for ArangoDB.

      User.create!(client, "testuser", passwd: "secret")
      User.grant_database_access!(client, "testuser", "mydb", :rw)
      User.grant_collection_access!(client, "testuser", "mydb", "docs", :ro)
      User.drop!(client, "testuser")
  """

  alias Toast.Client
  require Client
  import Toast.Utils, only: [maybe_put: 3]

  @spec list(Client.t()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client) do
    with {:ok, body} <- client |> Client.get("/_api/user/") |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @doc """
  Creates a user.

  ## Options

    * `:passwd` — the user's password
    * `:active` — whether the user is active (default: `true`)
    * `:extra` — map of additional user data
  """
  @spec create(Client.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def create(%Client{} = client, username, opts \\ []) do
    body = Map.put(build_user_body(opts), "user", username)
    client |> Client.post("/_api/user", body) |> Client.unwrap_ok()
  end

  @spec get(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, username) do
    client |> Client.get("/_api/user/#{username}") |> Client.unwrap()
  end

  @doc """
  Partially updates a user. Accepts `:passwd`, `:active`, and `:extra`.
  """
  @spec update(Client.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def update(%Client{} = client, username, opts) do
    client |> Client.patch("/_api/user/#{username}", build_user_body(opts)) |> Client.unwrap_ok()
  end

  @doc """
  Replaces a user. Accepts `:passwd`, `:active`, and `:extra`.
  """
  @spec replace(Client.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def replace(%Client{} = client, username, opts) do
    client |> Client.put("/_api/user/#{username}", build_user_body(opts)) |> Client.unwrap_ok()
  end

  @doc """
  Lists databases accessible by `username`.

  ## Options

    * `:full` — include permission details (default: `false`)
  """
  @spec list_databases(Client.t(), String.t(), keyword()) ::
          {:ok, map()} | {:error, term()}
  def list_databases(%Client{} = client, username, opts \\ []) do
    params = if Keyword.get(opts, :full, false), do: [full: true], else: []

    with {:ok, body} <-
           client
           |> Client.get("/_api/user/#{username}/database", params: params)
           |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(%Client{} = client, username) do
    Client.idempotent_delete(client, "/_api/user/#{username}")
  end

  @spec database_access(Client.t(), String.t(), String.t()) ::
          {:ok, String.t()} | {:error, term()}
  def database_access(%Client{} = client, username, database) do
    with {:ok, body} <-
           client |> Client.get("/_api/user/#{username}/database/#{database}") |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @spec grant_database_access(Client.t(), String.t(), String.t(), :rw | :ro | :none) ::
          :ok | {:error, term()}
  def grant_database_access(%Client{} = client, username, database, level) do
    body = %{"grant" => Atom.to_string(level)}

    client
    |> Client.put("/_api/user/#{username}/database/#{database}", body)
    |> Client.unwrap_ok()
  end

  @spec revoke_database_access(Client.t(), String.t(), String.t()) :: :ok | {:error, term()}
  def revoke_database_access(%Client{} = client, username, database) do
    client |> Client.delete("/_api/user/#{username}/database/#{database}") |> Client.unwrap_ok()
  end

  @spec collection_access(Client.t(), String.t(), String.t(), String.t()) ::
          {:ok, String.t()} | {:error, term()}
  def collection_access(%Client{} = client, username, database, collection) do
    with {:ok, body} <-
           client
           |> Client.get("/_api/user/#{username}/database/#{database}/#{collection}")
           |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @spec grant_collection_access(Client.t(), String.t(), String.t(), String.t(), :rw | :ro | :none) ::
          :ok | {:error, term()}
  def grant_collection_access(%Client{} = client, username, database, collection, level) do
    body = %{"grant" => Atom.to_string(level)}

    client
    |> Client.put("/_api/user/#{username}/database/#{database}/#{collection}", body)
    |> Client.unwrap_ok()
  end

  @spec revoke_collection_access(Client.t(), String.t(), String.t(), String.t()) ::
          :ok | {:error, term()}
  def revoke_collection_access(%Client{} = client, username, database, collection) do
    client
    |> Client.delete("/_api/user/#{username}/database/#{database}/#{collection}")
    |> Client.unwrap_ok()
  end

  defp build_user_body(opts) do
    %{}
    |> maybe_put("passwd", Keyword.get(opts, :passwd))
    |> maybe_put("active", Keyword.get(opts, :active))
    |> maybe_put("extra", Keyword.get(opts, :extra))
  end

  def list!(%Client{} = client), do: Client.bang!(list(client))

  def create!(%Client{} = client, username, opts \\ []),
    do: Client.bang!(create(client, username, opts))

  def get!(%Client{} = client, username), do: Client.bang!(get(client, username))

  def replace!(%Client{} = client, username, opts),
    do: Client.bang!(replace(client, username, opts))

  def update!(%Client{} = client, username, opts),
    do: Client.bang!(update(client, username, opts))

  def drop!(%Client{} = client, username), do: Client.bang!(drop(client, username))

  def list_databases!(%Client{} = client, username, opts \\ []),
    do: Client.bang!(list_databases(client, username, opts))

  def database_access!(%Client{} = client, username, database),
    do: Client.bang!(database_access(client, username, database))

  def grant_database_access!(%Client{} = client, username, database, level),
    do: Client.bang!(grant_database_access(client, username, database, level))

  def revoke_database_access!(%Client{} = client, username, database),
    do: Client.bang!(revoke_database_access(client, username, database))

  def collection_access!(%Client{} = client, username, database, collection),
    do: Client.bang!(collection_access(client, username, database, collection))

  def grant_collection_access!(%Client{} = client, username, database, collection, level),
    do: Client.bang!(grant_collection_access(client, username, database, collection, level))

  def revoke_collection_access!(%Client{} = client, username, database, collection),
    do: Client.bang!(revoke_collection_access(client, username, database, collection))
end
