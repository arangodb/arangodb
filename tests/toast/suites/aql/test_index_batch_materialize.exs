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

defmodule Aql.IndexBatchMaterializeTest do
  use Aql.Suite

  @db_name "BatchMaterializeIndexDB"
  @collection "MyTestCollection"
  @small_collection "MyTestCollectionSmall"
  @single_shard_collection "MySingleShardCollection"

  @batch_materialize_rule "batch-materialize-documents"

  setup_all %{client: client, deployment: deployment} do
    Client.Database.create!(client, @db_name)
    db = Client.with_database(client, @db_name)
    on_exit(fn -> Client.Database.drop(client, @db_name) end)

    create_and_index(db, @small_collection, 3)
    create_and_index(db, @collection, 3)
    create_and_index(db, @single_shard_collection, 1)

    fill_collection(db, @small_collection, 10)
    fill_collection(db, @collection, 5000)
    fill_collection(db, @single_shard_collection, 5000)

    %{db: db, is_cluster: Toast.Deployment.cluster?(deployment)}
  end

  defp create_and_index(client, name, num_shards) do
    Client.Collection.create!(client, name, number_of_shards: num_shards)

    if num_shards == 1 do
      Client.Index.ensure!(client, name, :persistent, ["v"], unique: true)
    end

    Client.Index.ensure!(client, name, :persistent, ["x"], stored_values: ["b", "z"])
    Client.Index.ensure!(client, name, :persistent, ["y"])
    Client.Index.ensure!(client, name, :persistent, ["z", "w"])
    Client.Index.ensure!(client, name, :persistent, ["u", "_key"], unique: true)
    Client.Index.ensure!(client, name, :persistent, ["p", "values[*].y"])
  end

  defp fill_collection(client, name, n) do
    0..(n - 1)
    |> Enum.map(fn i ->
      %{
        "_key" => Integer.to_string(i),
        "x" => i,
        "y" => 2 * i,
        "z" => 2 * i + 1,
        "w" => rem(i, 10),
        "u" => i,
        "b" => i + 1,
        "p" => i,
        "q" => i,
        "r" => i,
        "v" => i
      }
    end)
    |> Enum.chunk_every(1000)
    |> Enum.each(&Client.Document.insert!(client, name, &1))
  end

  defp normalize(nil), do: []

  defp normalize(projections) do
    projections
    |> Enum.map(fn
      %{"path" => path} -> path
      p -> List.wrap(p)
    end)
    |> Enum.sort()
  end

  defp explain(client, query, opts \\ %{}) do
    %{"plan" => plan} = Client.AQL.explain!(client, query, %{}, opts)
    plan
  end

  defp expect_node_order(nodes, earlier, later) do
    earlier_pos = Enum.find_index(nodes, &(&1["type"] == earlier))
    later_pos = Enum.find_index(nodes, &(&1["type"] == later))
    assert is_integer(earlier_pos), "expected #{earlier} in plan"
    assert is_integer(later_pos), "expected #{later} in plan"

    expect(earlier_pos < later_pos)
  end

  defp check_result(client, query, opts \\ %{}) do
    without_rule =
      Map.merge(opts, %{
        "optimizer" => %{"rules" => ["-#{@batch_materialize_rule}"]}
      })

    expected = Client.AQL.execute!(client, query, %{}, without_rule) |> Enum.sort()
    actual = Client.AQL.execute!(client, query, %{}, opts) |> Enum.sort()
    expect(actual == expected)
  end

  defp expect_no_optimization(client, query) do
    %{"rules" => rules, "nodes" => nodes} = explain(client, query)

    refute @batch_materialize_rule in rules
    assert Enum.any?(nodes, &(&1["type"] == "IndexNode")), "expected IndexNode in plan"
    refute Enum.any?(nodes, &(&1["type"] == "MaterializeNode"))
    refute Enum.any?(nodes, &(&1["strategy"] == "late materialized"))
  end

  defp expect_optimization(client, query, opts \\ %{}) do
    %{"rules" => rules, "nodes" => nodes} = explain(client, query, opts)

    expect(@batch_materialize_rule in rules)

    assert %{"outNmDocId" => %{"id" => doc_id}} =
             index_node =
             Enum.find(nodes, &(&1["strategy"] == "late materialized"))

    assert %{"inNmDocId" => %{"id" => ^doc_id}} =
             materialize_node =
             Enum.find(nodes, &(&1["type"] == "MaterializeNode"))

    %{nodes: nodes, index_node: index_node, materialize_node: materialize_node}
  end

  defp expect_double_optimization(client, query, opts \\ %{}) do
    %{"rules" => rules, "nodes" => nodes} = explain(client, query, opts)

    expect(@batch_materialize_rule in rules)
    assert [_, _] = index = Enum.filter(nodes, &(&1["type"] == "IndexNode"))
    assert [_, _] = materialize = Enum.filter(nodes, &(&1["type"] == "MaterializeNode"))

    %{index: index, materialize: materialize}
  end

  test "no materialize for small collections", %{db: client} do
    expect_no_optimization(client, """
    FOR doc IN #{@small_collection}
      FILTER doc.x > 5
      RETURN doc
    """)
  end

  test "no materialize small unique index", %{db: client} do
    expect_no_optimization(client, """
    FOR i IN 1..10
      FOR doc IN #{@collection}
        FILTER doc.x == i
        RETURN doc
    """)
  end

  test "materialize unique index", %{db: client} do
    query = """
    FOR i IN 1..120
      FOR doc IN #{@collection}
        FILTER doc.x == i
        RETURN doc
    """

    expect_optimization(client, query)
    check_result(client, query)
  end

  test "materialize unique index small range", %{db: client} do
    expect_no_optimization(client, """
    FOR i IN 1..10
      FOR doc IN #{@collection}
        FILTER doc.x == i
        RETURN doc
    """)
  end

  test "materialize index scan skip", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5
      SORT doc.x
      LIMIT 20, 10
      RETURN doc
    """

    expect_optimization(client, query)
    check_result(client, query)
  end

  test "materialize index scan skip 2", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.p > 5
      LIMIT 20, 10
      RETURN doc
    """

    expect_optimization(client, query)
    check_result(client, query)
  end

  test "materialize index scan skip 3", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.p > 5
      FILTER NOOPT(doc.q > 0)
      LIMIT 20, 10
      RETURN doc
    """

    expect_optimization(client, query)
    check_result(client, query)
  end

  test "materialize index scan full count", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5
      SORT doc.x
      LIMIT 0, 20
      RETURN doc
    """

    opts = %{"fullCount" => true}
    expect_optimization(client, query, opts)
    check_result(client, query, opts)
  end

  test "materialize index scan full count 2", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.p > 5
      LIMIT 0, 20
      RETURN doc
    """

    opts = %{"fullCount" => true}
    expect_optimization(client, query, opts)
    check_result(client, query, opts)
  end

  test "materialize index scan", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5
      RETURN doc
    """

    expect_optimization(client, query)
    check_result(client, query)
  end

  test "materialize multi index scan same index", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5 or doc.x < 8
      RETURN doc
    """

    expect_optimization(client, query)
    check_result(client, query)
  end

  test "materialize multi index scan multi index", %{db: client} do
    expect_no_optimization(client, """
    FOR doc IN #{@collection}
      FILTER doc.x > 5 or doc.y < 8
      RETURN doc
    """)
  end

  test "materialize sort stored values", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5
      SORT doc.b
      RETURN doc
    """

    %{materialize_node: mat, index_node: idx, nodes: nodes} =
      expect_optimization(client, query)

    check_result(client, query)
    expect(normalize(idx["projections"]) == [["b"]])
    expect(normalize(mat["projections"]) == [])
    expect_node_order(nodes, "SortNode", "MaterializeNode")
  end

  test "materialize filter stored values", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5
      FILTER NOOPT(doc.b > 7)
      RETURN doc
    """

    %{materialize_node: mat, index_node: idx, nodes: nodes} =
      expect_optimization(client, query)

    check_result(client, query)
    expect(normalize(idx["projections"]) == [["b"]])
    expect(normalize(mat["projections"]) == [])
    expect_node_order(nodes, "FilterNode", "MaterializeNode")
  end

  test "materialize stays above filter when maxProjections is too low", %{db: client} do
    query = """
    FOR doc IN #{@collection} OPTIONS {maxProjections: 1}
      FILTER doc.z == 5
      LET b = doc.w * 2 + doc.z * 3
      FILTER b < 5
      RETURN [doc, b]
    """

    %{materialize_node: mat, index_node: idx, nodes: nodes} =
      expect_optimization(client, query)

    check_result(client, query)
    expect(normalize(idx["projections"]) == [])
    expect(normalize(mat["projections"]) == [])
    expect_node_order(nodes, "MaterializeNode", "FilterNode")
  end

  test "materialize moves below filter when maxProjections is sufficient", %{db: client} do
    query = """
    FOR doc IN #{@collection} OPTIONS {maxProjections: 3}
      FILTER doc.z == 5
      LET b = doc.w * 2 + doc.z * 3
      FILTER b < 5
      RETURN [doc, b]
    """

    %{materialize_node: mat, index_node: idx, nodes: nodes} =
      expect_optimization(client, query)

    check_result(client, query)
    expect(normalize(idx["projections"]) == [["w"], ["z"]])
    expect(normalize(mat["projections"]) == [])
    expect_node_order(nodes, "FilterNode", "MaterializeNode")
  end

  test "materialize double filter stored values", %{db: client, is_cluster: is_cluster} do
    query = """
    FOR d1 IN #{@collection}
      FILTER d1.x > 5
      SORT d1.b
      LIMIT 100
      FILTER d1.z < 18
      RETURN d1.w
    """

    %{materialize_node: mat, index_node: idx, nodes: nodes} =
      expect_optimization(client, query)

    check_result(client, query)
    expect(normalize(idx["projections"]) == [["b"], ["z"]])
    expect(normalize(mat["projections"]) == [["w"]])
    expect_node_order(nodes, "SortNode", "MaterializeNode")

    if not is_cluster do
      expect_node_order(nodes, "FilterNode", "MaterializeNode")
    end
  end

  test "materialize index scan subquery", %{db: client, is_cluster: is_cluster} do
    query = """
    FOR d1 IN #{@collection}
      FILTER d1.x > 5
      LET e = SUM(FOR c IN #{@collection} LET p = d1.b + c.x LIMIT 10 RETURN p)
      SORT e
      LIMIT 10
      RETURN d1
    """

    %{materialize_node: mat, index_node: idx, nodes: nodes} =
      expect_optimization(client, query)

    check_result(client, query)

    if is_cluster do
      expect(normalize(idx["projections"]) == [])
      expect(normalize(mat["projections"]) == [])
    else
      expect(normalize(idx["projections"]) == [["b"]])
      expect(normalize(mat["projections"]) == [])
      expect_node_order(nodes, "LimitNode", "MaterializeNode")
      expect_node_order(nodes, "SortNode", "MaterializeNode")
      expect_node_order(nodes, "SubqueryEndNode", "MaterializeNode")
    end
  end

  test "materialization non-unique index", %{db: client} do
    query = """
    FOR dx IN #{@collection}
      FOR dy IN #{@collection}
        FILTER dx.x > 1 AND dx.x == dy.x
        RETURN [dy, dx]
    """

    assert %{
             index: [%{"id" => id1}, %{"id" => id2}],
             materialize: [%{"dependencies" => [id1 | _]}, %{"dependencies" => [id2 | _]}]
           } = expect_double_optimization(client, query)

    check_result(client, query)
  end

  test "materialization unique index chains when single shard", %{db: client} do
    query = """
    FOR dx IN #{@single_shard_collection}
      FOR dy IN #{@single_shard_collection}
        FILTER dx.v > 1 AND dx.v == dy.v
        RETURN [dx, dy]
    """

    # chained: index -> index -> materialize -> materialize
    assert %{
             index: [%{"id" => id1}, %{"id" => id2, "dependencies" => [id1 | _]}],
             materialize: [
               %{"id" => id3, "dependencies" => [id2 | _]},
               %{"dependencies" => [id3 | _]}
             ]
           } = expect_double_optimization(client, query)

    check_result(client, query)
  end

  test "pushDownMaterialization true chains on single shard", %{db: client} do
    query = """
    FOR dx IN #{@single_shard_collection}
      FOR dy IN #{@single_shard_collection} OPTIONS { pushDownMaterialization: true }
        FILTER dx.x > 1 AND dx.x == dy.x
        RETURN [dx, dy]
    """

    # chained: index -> index -> materialize -> materialize
    assert %{
             index: [%{"id" => id1}, %{"id" => id2, "dependencies" => [id1 | _]}],
             materialize: [
               %{"id" => id3, "dependencies" => [id2 | _]},
               %{"dependencies" => [id3 | _]}
             ]
           } = expect_double_optimization(client, query)

    check_result(client, query)
  end

  test "pushDownMaterialization false keeps interleaved", %{db: client} do
    query = """
    FOR dx IN #{@collection}
      FOR dy IN #{@collection} OPTIONS { pushDownMaterialization: false}
        FILTER dx.x > 1 AND dx.x == dy.x
        RETURN [dy, dx]
    """

    assert %{
             index: [%{"id" => id1}, %{"id" => id2}],
             materialize: [%{"dependencies" => [id1 | _]}, %{"dependencies" => [id2 | _]}]
           } = expect_double_optimization(client, query)

    check_result(client, query)
  end

  test "materialization tuple unique index stays interleaved", %{db: client} do
    query = """
    FOR dx IN #{@collection}
      FOR dy IN #{@collection}
        FILTER dx.u > 1 AND dx.u == dy.u
        RETURN [dx, dy]
    """

    assert %{
             index: [%{"id" => id1}, %{"id" => id2}],
             materialize: [%{"dependencies" => [id1 | _]}, %{"dependencies" => [id2 | _]}]
           } = expect_double_optimization(client, query)

    check_result(client, query)
  end

  test "materialize index scan subquery full doc", %{db: client} do
    query = """
    FOR d1 IN #{@collection}
      FILTER d1.x > 5
      LET e = SUM(FOR c IN #{@collection} LET p = d1 LIMIT 10 RETURN p)
      SORT e
      LIMIT 10
      RETURN d1
    """

    %{index_node: idx, materialize_node: mat, nodes: nodes} =
      expect_optimization(client, query)

    check_result(client, query)
    expect(normalize(idx["projections"]) == [])
    expect(normalize(mat["projections"]) == [])
    expect_node_order(nodes, "MaterializeNode", "LimitNode")
    expect_node_order(nodes, "MaterializeNode", "SortNode")
    expect_node_order(nodes, "MaterializeNode", "SubqueryEndNode")
  end

  test "materialize index scan projections", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5
      RETURN [doc.y, doc.z, doc.a]
    """

    %{materialize_node: mat, index_node: idx} = expect_optimization(client, query)
    check_result(client, query)
    expect(normalize(idx["projections"]) == [])
    expect(normalize(mat["projections"]) == [["a"], ["y"], ["z"]])
  end

  test "materialize index scan covering projections", %{db: client} do
    expect_no_optimization(client, """
    FOR doc IN #{@collection}
      FILTER doc.x > 5
      RETURN [doc.x, doc.b]
    """)
  end

  test "materialize index scan post filter covered", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5 AND doc.b < 7
      RETURN doc
    """

    expect_optimization(client, query)
    check_result(client, query)
  end

  test "materialize index scan post filter not covered", %{db: client} do
    expect_no_optimization(client, """
    FOR doc IN #{@collection}
      FILTER doc.x > 5 AND doc.c < 7
      RETURN doc
    """)
  end

  test "materialize skip index scan post filter covered", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5 AND doc.b < 7
      LIMIT 10, 20
      RETURN doc
    """

    expect_optimization(client, query)
    check_result(client, query)
  end

  test "materialize skip index scan post filter not covered", %{db: client} do
    expect_no_optimization(client, """
    FOR doc IN #{@collection}
      FILTER doc.x > 5 AND doc.c < 7
      LIMIT 10, 20
      RETURN doc
    """)
  end

  test "materialize index scan post filter dependent var", %{db: client} do
    query = """
    FOR i IN 1..5
      FOR doc IN #{@collection}
        FILTER doc.x > 5 AND doc.b < i
        RETURN doc
    """

    %{index_node: idx} = expect_optimization(client, query)
    check_result(client, query)
    expect(idx["indexCoversFilterProjections"])
    expect(normalize(idx["filterProjections"]) == [["b"]])
  end

  test "materialize index scan no projection optimization", %{db: client} do
    query = """
    FOR doc IN #{@collection}
      FILTER doc.x > 5
      RETURN doc.r
    """

    expect_optimization(client, query, %{
      "optimizer" => %{"rules" => ["-optimize-projections"]}
    })

    check_result(client, query)
    expect(Client.AQL.execute!(client, query) |> Enum.sort() == Enum.to_list(6..4999))
  end
end
