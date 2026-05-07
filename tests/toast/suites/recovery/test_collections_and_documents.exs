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

defmodule Recovery.CollectionsAndDocumentsTest do
  @moduledoc """
  Recovery tests for basic collection and document durability.

  Ported from:
    - tests/js/client/recovery/create-collections.js
    - tests/js/client/recovery/drop-collections.js
    - tests/js/client/recovery/insert-update-replace.js
    - tests/js/client/recovery/edges.js
  """

  use Recovery.Suite

  @edge_collection_type 3

  test "collections with various types and properties survive crash" do
    with_deployment(fn deployment ->
      client = default_client!(deployment)

      Client.Collection.create!(client, "recovery_sync", wait_for_sync: true)

      Client.Document.insert!(client, "recovery_sync", %{
        "value1" => 1,
        "value2" => ["the", "quick", "brown", "foxx"]
      })

      Client.Index.ensure!(client, "recovery_sync", :persistent, ["value1"])
      Client.Index.ensure!(client, "recovery_sync", :persistent, ["value2"])

      Client.Collection.create!(client, "recovery_nosync")
      Client.Document.insert!(client, "recovery_nosync", %{"value1" => %{"some" => "data"}})
      Client.Index.ensure!(client, "recovery_nosync", :persistent, ["value1"])

      Client.Collection.create_edge!(client, "recovery_edges")

      Client.Document.insert!(client, "recovery_edges", %{
        "_from" => "recovery_sync/a",
        "_to" => "recovery_nosync/b",
        "value1" => %{"some" => "data"}
      })

      Client.Index.ensure!(client, "recovery_edges", :persistent, ["value1"], unique: true)

      Client.Collection.create!(client, "_recovery_system", is_system: true)
      Client.Document.insert!(client, "_recovery_system", %{"value42" => 42})
      Client.Index.ensure!(client, "_recovery_system", :persistent, ["value42"], unique: true)

      Client.Document.insert!(client, "recovery_sync", %{"_key" => "sync"}, wait_for_sync: true)

      crash_and_recover!(deployment)

      expect(Client.Collection.count!(client, "recovery_sync") == 2)
      expect(%{"waitForSync" => true} = Client.Collection.properties!(client, "recovery_sync"))
      expect(length(Client.Index.list!(client, "recovery_sync")) == 3)

      expect(Client.Collection.count!(client, "recovery_nosync") == 1)
      expect(%{"waitForSync" => false} = Client.Collection.properties!(client, "recovery_nosync"))
      expect(length(Client.Index.list!(client, "recovery_nosync")) == 2)

      expect(Client.Collection.count!(client, "recovery_edges") == 1)
      expect(%{"type" => @edge_collection_type} = Client.Collection.properties!(client, "recovery_edges"))
      expect("edge" in Enum.map(Client.Index.list!(client, "recovery_edges"), & &1["type"]))

      expect(Client.Collection.count!(client, "_recovery_system") == 1)
      expect(%{"isSystem" => true} = Client.Collection.properties!(client, "_recovery_system"))
    end)
  end

  test "dropped collections stay dropped after crash" do
    with_deployment(fn deployment ->
      client = default_client!(deployment)

      for i <- 0..4, do: Client.Collection.create!(client, "recovery_drop_#{i}")

      for i <- 0..3, do: Client.Collection.drop!(client, "recovery_drop_#{i}")

      Client.Document.insert!(client, "recovery_drop_4", %{"_key" => "sync"}, wait_for_sync: true)

      crash_and_recover!(deployment)

      for i <- 0..3 do
        expect({:error, _} = Client.Collection.info(client, "recovery_drop_#{i}"))
      end

      expect(Client.Collection.count!(client, "recovery_drop_4") == 1)
    end)
  end

  test "insert and replace operations persist across crash" do
    with_deployment(fn deployment ->
      client = default_client!(deployment)

      Client.Collection.create!(client, "recovery_docs")

      # Interleave insert+replace per document to produce many small WAL entries —
      # this exercises the WAL replay path for interleaved operations on the same key.
      for i <- 0..999 do
        Client.Document.insert!(client, "recovery_docs", %{
          "_key" => "doc#{i}",
          "value1" => i,
          "value2" => "test#{i}"
        })

        Client.Document.replace!(client, "recovery_docs", "doc#{i}", %{
          "replaced" => i,
          "new_field" => "val#{i}"
        })
      end

      Client.Document.insert!(client, "recovery_docs", %{"_key" => "sync"}, wait_for_sync: true)

      crash_and_recover!(deployment)

      expect(Client.Collection.count!(client, "recovery_docs") == 1001)

      docs =
        Client.AQL.execute!(client, """
        FOR d IN recovery_docs
          FILTER d._key != 'sync'
          SORT d.replaced ASC
          RETURN d
        """)

      expect(length(docs) == 1000)

      for {doc, i} <- Enum.with_index(docs) do
        expected_field = "val#{i}"
        expect %{"replaced" => ^i, "new_field" => ^expected_field} = doc
      end
    end)
  end

  test "edge documents with graph structure survive crash" do
    with_deployment(fn deployment ->
      client = default_client!(deployment)

      Client.Collection.create!(client, "recovery_vertices")
      Client.Collection.create_edge!(client, "recovery_edge_data")

      vertices =
        Enum.map(0..999, fn i ->
          %{"_key" => "node#{i}", "name" => "some-name#{i}"}
        end)

      Client.Document.insert_many!(client, "recovery_vertices", vertices)

      edges =
        Enum.map(0..999, fn i ->
          %{
            "_key" => "edge#{i}",
            "_from" => "recovery_vertices/node#{i}",
            "_to" => "recovery_vertices/node#{rem(i, 10)}",
            "what" => "some-value#{i}"
          }
        end)

      Client.Document.insert_many!(client, "recovery_edge_data", edges)

      Client.Document.insert!(client, "recovery_vertices", %{"_key" => "sync"},
        wait_for_sync: true
      )

      crash_and_recover!(deployment)

      expect(Client.Collection.count!(client, "recovery_vertices") == 1001)

      recovered_vertices =
        Client.AQL.execute!(client, """
        FOR v IN recovery_vertices
          FILTER v._key != 'sync'
          SORT v._key ASC
          RETURN v
        """)

      expect(length(recovered_vertices) == 1000)

      for {v, i} <- Enum.with_index(recovered_vertices) do
        expect(v["name"] == "some-name#{i}")
      end

      recovered_edges =
        Client.AQL.execute!(client, """
        FOR e IN recovery_edge_data
          SORT e._key ASC
          RETURN e
        """)

      expect(length(recovered_edges) == 1000)

      for {e, i} <- Enum.with_index(recovered_edges) do
        expect(e["what"] == "some-value#{i}")
        expect(e["_from"] == "recovery_vertices/node#{i}")
        expect(e["_to"] == "recovery_vertices/node#{rem(i, 10)}")
      end

      outgoing =
        Client.AQL.execute!(
          client,
          "FOR e IN recovery_edge_data FILTER e._from == 'recovery_vertices/node42' RETURN e"
        )

      expect(length(outgoing) == 1)

      incoming =
        Client.AQL.execute!(
          client,
          "FOR e IN recovery_edge_data FILTER e._to == 'recovery_vertices/node3' RETURN e"
        )

      expect(length(incoming) == 100)
    end)
  end
end
