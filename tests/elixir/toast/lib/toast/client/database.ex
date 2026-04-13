defmodule Toast.Client.Database do
  @moduledoc """
  Database management operations for ArangoDB.

      Database.create!(client, "testdb", users: [%{"username" => "root"}])
      db_client = %{client | database: "testdb"}
      Database.drop!(client, "testdb")
  """

  alias Toast.Client
  require Client
  import Toast.Utils, only: [maybe_put: 3]

  @spec list(Client.t()) :: {:ok, [String.t()]} | {:error, term()}
  def list(%Client{} = client) do
    with {:ok, body} <- client |> Client.get("/_api/database") |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @spec list_accessible(Client.t()) :: {:ok, [String.t()]} | {:error, term()}
  def list_accessible(%Client{} = client) do
    with {:ok, body} <- client |> Client.get("/_api/database/user") |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @spec current(Client.t()) :: {:ok, map()} | {:error, term()}
  def current(%Client{} = client) do
    with {:ok, body} <- client |> Client.get("/_api/database/current") |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @doc """
  Creates a database.

  ## Options

    * `:users` — list of user objects to grant access to the new database
    * `:options` — map of database creation options (e.g., replication factor)
  """
  @spec create(Client.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def create(%Client{} = client, name, opts \\ []) do
    body =
      %{"name" => name}
      |> maybe_put("users", Keyword.get(opts, :users))
      |> maybe_put("options", Keyword.get(opts, :options))

    client |> Client.post("/_api/database", body) |> Client.unwrap_ok()
  end

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(%Client{} = client, name) do
    Client.idempotent_delete(client, "/_api/database/#{name}")
  end

  def list!(%Client{} = client), do: Client.bang!(list(client))
  def list_accessible!(%Client{} = client), do: Client.bang!(list_accessible(client))
  def current!(%Client{} = client), do: Client.bang!(current(client))
  def create!(%Client{} = client, name, opts \\ []), do: Client.bang!(create(client, name, opts))
  def drop!(%Client{} = client, name), do: Client.bang!(drop(client, name))
end
