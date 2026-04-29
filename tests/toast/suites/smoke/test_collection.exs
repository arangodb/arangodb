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

defmodule Smoke.CollectionTest do
  use Smoke.Suite

  setup %{client: client} do
    name = "test_coll_#{System.unique_integer([:positive])}"
    on_exit(fn -> Client.Collection.drop(client, name) end)
    %{collection: name}
  end

  test "create and list collection", %{client: client, collection: name} do
    assert {:ok, _} = Client.Collection.create(client, name)
    assert {:ok, collections} = Client.Collection.list(client, exclude_system: true)
    assert Enum.any?(collections, &(&1["name"] == name))
  end

  test "drop collection", %{client: client, collection: name} do
    assert {:ok, _} = Client.Collection.create(client, name)
    assert :ok = Client.Collection.drop(client, name)
    assert {:ok, collections} = Client.Collection.list(client, exclude_system: true)
    refute Enum.any?(collections, &(&1["name"] == name))
  end

  test "insert and get document", %{client: client, collection: name} do
    assert {:ok, _} = Client.Collection.create(client, name)
    assert {:ok, meta} = Client.Document.insert(client, name, %{"value" => 42})
    assert {:ok, doc} = Client.Document.get(client, name, meta["_key"])
    assert doc["value"] == 42
  end

  test "insert and remove document", %{client: client, collection: name} do
    assert {:ok, _} = Client.Collection.create(client, name)
    assert {:ok, meta} = Client.Document.insert(client, name, %{"hello" => "world"})
    assert :ok = Client.Document.remove(client, name, meta["_key"])
    assert {:error, %{status: 404}} = Client.Document.get(client, name, meta["_key"])
  end
end
