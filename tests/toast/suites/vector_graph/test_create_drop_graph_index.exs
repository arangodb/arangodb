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

defmodule VectorGraph.CreateDropGraphIndexTest do
  use VectorGraph.Suite

  # Vector-graph indexes only need the vector dimension (a multiple of 32) and
  # a distance metric — there is no nLists/training as with the IVF index.
  @params %{"dimension" => 32, "metric" => "l2"}

  setup %{client: client} do
    name = "vg_coll_#{System.unique_integer([:positive])}"
    assert {:ok, _} = Client.Collection.create(client, name)
    on_exit(fn -> Client.Collection.drop(client, name) end)
    %{collection: name}
  end

  test "create index reports vector-graph type", %{client: client, collection: coll} do
    assert {:ok, index} =
             Client.Index.ensure(client, coll, :"vector-graph", ["vector"],
               name: "vg_idx",
               params: @params
             )

    assert index["type"] == "vector-graph"
    assert index["fields"] == ["vector"]
  end

  test "created index appears in the index list", %{client: client, collection: coll} do
    assert {:ok, _} =
             Client.Index.ensure(client, coll, :"vector-graph", ["vector"],
               name: "vg_idx",
               params: @params
             )

    assert {:ok, indexes} = Client.Index.list(client, coll)
    assert Enum.any?(indexes, &(&1["type"] == "vector-graph" and &1["name"] == "vg_idx"))
  end

  test "drop index removes it from the collection", %{client: client, collection: coll} do
    assert {:ok, index} =
             Client.Index.ensure(client, coll, :"vector-graph", ["vector"],
               name: "vg_idx",
               params: @params
             )

    assert :ok = Client.Index.drop(client, index["id"])

    assert {:ok, indexes} = Client.Index.list(client, coll)
    refute Enum.any?(indexes, &(&1["type"] == "vector-graph"))
  end

  test "dimension must be a multiple of 32", %{client: client, collection: coll} do
    assert {:error, %{status: 400}} =
             Client.Index.ensure(client, coll, :"vector-graph", ["vector"],
               params: %{"dimension" => 30, "metric" => "l2"}
             )
  end

  test "index cannot be unique", %{client: client, collection: coll} do
    assert {:error, %{status: 400}} =
             Client.Index.ensure(client, coll, :"vector-graph", ["vector"],
               unique: true,
               params: @params
             )
  end
end
