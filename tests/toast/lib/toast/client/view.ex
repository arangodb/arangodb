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

defmodule Toast.Client.View do
  @moduledoc """
  ArangoSearch view management.

      View.create!(client, "my_view", "arangosearch", %{
        "links" => %{"docs" => %{"includeAllFields" => true}}
      })
      {:ok, props} = View.properties(client, "my_view")
      View.drop!(client, "my_view")
  """

  alias Toast.Client
  require Client

  @spec list(Client.t()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client) do
    with {:ok, body} <- client |> Client.get("/_api/view") |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @spec create(Client.t(), String.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def create(%Client{} = client, name, type, properties \\ %{}) do
    body = Map.merge(properties, %{"name" => name, "type" => type})
    client |> Client.post("/_api/view", body) |> Client.unwrap()
  end

  @spec info(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def info(%Client{} = client, name) do
    client |> Client.get("/_api/view/#{name}") |> Client.unwrap()
  end

  @spec properties(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def properties(%Client{} = client, name) do
    client |> Client.get("/_api/view/#{name}/properties") |> Client.unwrap()
  end

  @spec update_properties(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def update_properties(%Client{} = client, name, props) do
    client |> Client.patch("/_api/view/#{name}/properties", props) |> Client.unwrap()
  end

  @spec replace_properties(Client.t(), String.t(), map()) :: {:ok, map()} | {:error, term()}
  def replace_properties(%Client{} = client, name, props) do
    client |> Client.put("/_api/view/#{name}/properties", props) |> Client.unwrap()
  end

  @spec drop(Client.t(), String.t()) :: :ok | {:error, term()}
  def drop(%Client{} = client, name) do
    Client.idempotent_delete(client, "/_api/view/#{name}")
  end

  def list!(%Client{} = client), do: Client.bang!(list(client))

  def create!(%Client{} = client, name, type, properties \\ %{}),
    do: Client.bang!(create(client, name, type, properties))

  def info!(%Client{} = client, name), do: Client.bang!(info(client, name))
  def properties!(%Client{} = client, name), do: Client.bang!(properties(client, name))

  def update_properties!(%Client{} = client, name, props),
    do: Client.bang!(update_properties(client, name, props))

  def replace_properties!(%Client{} = client, name, props),
    do: Client.bang!(replace_properties(client, name, props))

  def drop!(%Client{} = client, name), do: Client.bang!(drop(client, name))
end
