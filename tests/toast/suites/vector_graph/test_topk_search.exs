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

defmodule VectorGraph.TopKSearchTest do
  use VectorGraph.Suite

  @dimension 32
  @doc_count 64

  # Each document lives at a distinct position on the first axis (all other
  # components are zero), so the L2 nearest neighbours of point(i) are the
  # documents with the closest index — an easy, deterministic ground truth.
  defp point(i), do: [i * 1.0 | List.duplicate(0.0, @dimension - 1)]

  defp search(client, coll, query, k) do
    aql =
      "FOR d IN @@coll SORT APPROX_NEAR_L2(d.vector, @q) LIMIT #{k} RETURN d._key"

    Client.AQL.execute!(client, aql, %{"@coll" => coll, "q" => query})
  end

  setup %{client: client} do
    name = "vg_search_#{System.unique_integer([:positive])}"
    assert {:ok, _} = Client.Collection.create(client, name)

    assert {:ok, _} =
             Client.Index.ensure(client, name, :"vector-graph", ["vector"],
               name: "vg_idx",
               params: %{"dimension" => @dimension, "metric" => "l2"}
             )

    docs = for i <- 0..(@doc_count - 1), do: %{"_key" => "p#{i}", "vector" => point(i)}
    assert {:ok, _} = Client.Document.insert_many(client, name, docs)

    on_exit(fn -> Client.Collection.drop(client, name) end)
    %{collection: name}
  end

  test "topK returns exactly K documents", %{client: client, collection: coll} do
    assert length(search(client, coll, point(20), 10)) == 10
  end

  test "querying an indexed point returns it as the nearest", %{client: client, collection: coll} do
    assert ["p5"] = search(client, coll, point(5), 1)
  end

  test "topK returns the K nearest neighbours", %{client: client, collection: coll} do
    # point(5)'s neighbours are p4 and p6 (both at distance 1), then p3/p7.
    keys = search(client, coll, point(5), 3)
    assert length(keys) == 3
    assert MapSet.new(keys) == MapSet.new(["p5", "p4", "p6"])
  end

  test "LIMIT larger than the collection returns every document", %{
    client: client,
    collection: coll
  } do
    assert length(search(client, coll, point(0), @doc_count * 10)) == @doc_count
  end
end
