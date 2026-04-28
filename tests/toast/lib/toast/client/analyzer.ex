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

defmodule Toast.Client.Analyzer do
  @moduledoc """
  Analyzer management for ArangoDB.

      Analyzer.create!(client, "my_text", "text",
        properties: %{"locale" => "en", "stemming" => true},
        features: ["frequency", "position"])
      {:ok, analyzer} = Analyzer.get(client, "my_text")
      Analyzer.drop!(client, "my_text", force: true)
  """

  alias Toast.Client
  require Client
  import Toast.Client.Utils, only: [translate_opts: 2]

  @create_body_keys %{properties: "properties", features: "features"}

  @spec list(Client.t()) :: {:ok, [map()]} | {:error, term()}
  def list(%Client{} = client) do
    with {:ok, body} <- client |> Client.get("/_api/analyzer") |> Client.unwrap() do
      {:ok, body["result"]}
    end
  end

  @doc """
  Creates an analyzer.

  ## Options

    * `:properties` — analyzer-specific configuration map
    * `:features` — list of features (e.g., `["frequency", "position"]`)
  """
  @spec create(Client.t(), String.t(), String.t(), keyword()) :: {:ok, map()} | {:error, term()}
  def create(%Client{} = client, name, type, opts \\ []) do
    extra = translate_opts(opts, @create_body_keys) |> Map.new()
    body = Map.merge(%{"name" => name, "type" => type}, extra)
    client |> Client.post("/_api/analyzer", body) |> Client.unwrap()
  end

  @spec get(Client.t(), String.t()) :: {:ok, map()} | {:error, term()}
  def get(%Client{} = client, name) do
    client |> Client.get("/_api/analyzer/#{name}") |> Client.unwrap()
  end

  @doc """
  Drops an analyzer. Returns `:ok` even if the analyzer does not exist.

  ## Options

    * `:force` — remove even if in use by a view
  """
  @spec drop(Client.t(), String.t(), keyword()) :: :ok | {:error, term()}
  def drop(%Client{} = client, name, opts \\ []) do
    params = if Keyword.get(opts, :force), do: [force: true], else: []
    Client.idempotent_delete(client, "/_api/analyzer/#{name}", params: params)
  end

  def list!(%Client{} = client), do: Client.bang!(list(client))

  def create!(%Client{} = client, name, type, opts \\ []),
    do: Client.bang!(create(client, name, type, opts))

  def get!(%Client{} = client, name), do: Client.bang!(get(client, name))
  def drop!(%Client{} = client, name, opts \\ []), do: Client.bang!(drop(client, name, opts))
end
