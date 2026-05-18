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

defmodule Aql.IndexJoinTest do
  use Aql.Suite

  @db_name "IndexJoinDB"

  @query_options %{
    "optimizer" => %{
      "rules" => ["+join-index-nodes", "-replace-equal-attribute-accesses"]
    },
    "maxNumberOfPlans" => 1
  }

  setup %{client: client, deployment: deployment} do
    Client.Database.create!(client, @db_name)
    on_exit(fn -> Client.Database.drop(client, @db_name) end)
    db_client = Client.with_database(client, @db_name)
    %{db: db_client, is_cluster: Toast.Deployment.cluster?(deployment)}
  end

  defp setup_collection(client, name, docs) do
    Client.Collection.create!(client, name)
    insert_docs(client, name, docs)
  end

  defp insert_docs(client, collection, docs) do
    docs
    |> Enum.chunk_every(1000)
    |> Enum.each(&Client.Document.insert!(client, collection, &1))
  end

  @duplicate_name 1207

  defp ensure_prototype(client, name) do
    case Client.Collection.create(client, name, number_of_shards: 3) do
      {:ok, _} -> :ok
      {:error, %{body: %{"errorNum" => @duplicate_name}}} -> :ok
    end
  end

  defp create_collection(client, is_cluster, name, opts \\ [])

  defp create_collection(client, true, name, opts) do
    shard_keys = Keyword.get(opts, :shard_keys, ["x"])
    prototype = Keyword.get(opts, :prototype, "prototype")
    ensure_prototype(client, prototype)

    Client.Collection.create!(client, name,
      number_of_shards: 3,
      shard_keys: shard_keys,
      distribute_shards_like: prototype
    )
  end

  defp create_collection(client, false, name, _opts), do: Client.Collection.create!(client, name)

  defp create_edge_collection(client, true, name) do
    ensure_prototype(client, "prototype")

    Client.Collection.create_edge!(client, name,
      number_of_shards: 3,
      shard_keys: ["x"],
      distribute_shards_like: "prototype"
    )
  end

  defp create_edge_collection(client, false, name),
    do: Client.Collection.create_edge!(client, name)

  defp explain_join(client, query, query_opts \\ @query_options) do
    %{"plan" => %{"nodes" => nodes}} = Client.AQL.explain!(client, query, %{}, query_opts)
    join = Enum.find(nodes, &(&1["type"] == "JoinNode"))
    {nodes, join}
  end

  defp explain_join!(client, query, query_opts \\ @query_options) do
    {nodes, join} = explain_join(client, query, query_opts)
    assert join, "Expected JoinNode in plan, got: #{inspect(Enum.map(nodes, & &1["type"]))}"
    join
  end

  defp execute_join_query(client, query) do
    explain_join!(client, query)
    Client.AQL.execute!(client, query, %{}, @query_options)
  end

  defp normalize(projections) do
    projections
    |> Enum.map(fn
      %{"path" => path} -> path
      p -> List.wrap(p)
    end)
    |> Enum.sort()
  end

  defp assert_projections(%{"indexInfos" => infos}, expected) do
    infos
    |> Enum.zip(expected)
    |> Enum.with_index()
    |> Enum.each(fn {{info, expected_proj}, idx} ->
      assert normalize(info["projections"]) == normalize(expected_proj),
             "indexInfos[#{idx}].projections mismatch"
    end)
  end

  defp assert_join_matches(result, expected_count) do
    expect length(result) == expected_count
    Enum.each(result, fn [%{"x" => x}, %{"x" => y}] -> expect x == y end)
  end

  test "join with sort", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1 * 2}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    result =
      execute_join_query(client, """
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
            FILTER doc1.x == doc2.x
            SORT doc2.x
            RETURN [doc1, doc2]
      """)

    expect length(result) == 5

    Enum.reduce(result, -1, fn [%{"x" => x}, %{"x" => y}], prev ->
      expect x == y
      expect x > prev
      x
    end)
  end

  test "even odd - no matches", %{db: client} do
    setup_collection(client, "A", Enum.map(0..999, &%{"x" => 2 * &1 + 1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..999, &%{"x" => 2 * &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    result =
      execute_join_query(client, """
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
            FILTER doc1.x == doc2.x
            RETURN [doc1, doc2]
      """)

    expect result == []
  end

  test "every other match small", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..4, &%{"x" => 2 * &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    result =
      execute_join_query(client, """
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
            FILTER doc1.x == doc2.x
            RETURN [doc1, doc2]
      """)

    assert_join_matches(result, 5)
  end

  test "every other match medium", %{db: client} do
    setup_collection(client, "A", Enum.map(0..999, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..499, &%{"x" => 2 * &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    result =
      execute_join_query(client, """
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
            FILTER doc1.x == doc2.x
            RETURN [doc1, doc2]
      """)

    assert_join_matches(result, 500)
  end

  test "every other match big", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9999, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..4999, &%{"x" => 2 * &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    result =
      execute_join_query(client, """
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
            FILTER doc1.x == doc2.x
            RETURN [doc1, doc2]
      """)

    assert_join_matches(result, 5000)
  end

  test "every multiple matches", %{db: client} do
    setup_collection(client, "A", Enum.map(0..99, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..999, &%{"x" => rem(&1, 100)}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    result =
      execute_join_query(client, """
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
            FILTER doc1.x == doc2.x
            RETURN [doc1, doc2]
      """)

    assert_join_matches(result, 1000)
  end

  test "every multiple matches unique index", %{db: client} do
    setup_collection(client, "A", Enum.map(0..99, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"], unique: true)
    setup_collection(client, "B", Enum.map(0..999, &%{"x" => rem(&1, 100)}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    result =
      execute_join_query(client, """
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
            FILTER doc1.x == doc2.x
            RETURN [doc1, doc2]
      """)

    assert_join_matches(result, 1000)
  end

  test "full product", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, fn _ -> %{"x" => 0} end))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..9, fn _ -> %{"x" => 0} end))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    result =
      execute_join_query(client, """
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
            FILTER doc1.x == doc2.x
            RETURN [doc1, doc2]
      """)

    assert_join_matches(result, 100)
  end

  test "projections", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN [doc1.x, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [["x"], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn [a, b] -> expect a == b end)
  end

  test "projections unique index", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"], unique: true)
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN [doc1.x, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [["x"], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn [a, b] -> expect a == b end)
  end

  test "projections from stored value", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"], stored_values: ["y"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN [doc1.y, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [["y"], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn [y, x] -> expect y == 2 * x end)
  end

  test "projections from stored value unique index", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"], stored_values: ["y"], unique: true)
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN [doc1.y, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [["y"], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn [y, x] -> expect y == 2 * x end)
  end

  test "projections from key field", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x", "y"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN [doc1.y, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [["y"], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn [y, x] -> expect y == 2 * x end)
  end

  test "projections from key field unique index", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x", "y"], unique: true)
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN [doc1.y, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [["y"], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn [y, x] -> expect y == 2 * x end)
  end

  test "projections from document", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN [doc1.y, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [["y"], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn [y, x] -> expect y == 2 * x end)
  end

  test "projections optimized away", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN 1
    """

    assert %{
             "indexInfos" => [
               %{"producesOutput" => false, "projections" => []},
               %{"producesOutput" => false, "projections" => []}
             ]
           } = explain_join!(client, query)

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    expect Enum.all?(result, &(&1 == 1))
  end

  test "projections first optimized away", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          RETURN doc2.y
    """

    assert %{
             "indexInfos" => [
               %{"producesOutput" => false, "projections" => []},
               %{"producesOutput" => true, "projections" => projections}
             ]
           } = explain_join!(client, query)

    expect normalize(projections) == normalize(["y"])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn y -> expect rem(y, 2) == 0 end)
  end

  test "projections not optimized away - filter on first", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => 2 * &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => 2 * &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x FILTER doc1.y >= 0
          RETURN doc2.x
    """

    assert %{
             "indexInfos" => [
               %{
                 "producesOutput" => false,
                 "projections" => [],
                 "filterProjections" => filter_proj
               },
               %{"producesOutput" => true, "projections" => projections}
             ]
           } = explain_join!(client, query)

    expect normalize(filter_proj) == normalize(["y"])
    expect normalize(projections) == normalize(["x"])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn x -> expect rem(x, 2) == 0 end)
  end

  test "projections not optimized away - filter on both", %{db: client} do
    setup_collection(client, "A", Enum.map(0..9, &%{"x" => 2 * &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..9, &%{"x" => 2 * &1, "y" => 2 * &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x FILTER doc1.y >= 0 FILTER doc2.y >= 0
          RETURN doc2.x
    """

    %{
      "indexInfos" => [
        %{
          "producesOutput" => false,
          "projections" => [],
          "filterProjections" => first_filter
        },
        %{
          "producesOutput" => true,
          "projections" => second_proj,
          "filterProjections" => second_filter
        }
      ]
    } = explain_join!(client, query)

    expect normalize(first_filter) == normalize(["y"])
    expect normalize(second_proj) == normalize(["x"])
    expect normalize(second_filter) == normalize(["y"])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 10
    Enum.each(result, fn x -> expect rem(x, 2) == 0 end)
  end

  test "join with document post filter", %{db: client} do
    setup_collection(client, "A", Enum.map(0..19, &%{"x" => 2 * &1, "y" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..19, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          FILTER doc1.y % 2 == 0
          RETURN [doc1.x, doc1.y, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [["x", "y"], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 5

    Enum.each(result, fn [x1, y, x2] ->
      expect x1 == x2
      expect rem(y, 2) == 0
    end)
  end

  test "join with document post filter produce document", %{db: client} do
    setup_collection(client, "A", Enum.map(0..19, &%{"x" => 2 * &1, "y" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    setup_collection(client, "B", Enum.map(0..19, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          FILTER doc1.y % 2 == 0
          RETURN [doc1, doc2.x]
    """

    join = explain_join!(client, query)
    assert_projections(join, [[], ["x"]])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 5

    Enum.each(result, fn [%{"x" => x, "y" => y}, x2] ->
      expect x == x2
      expect rem(y, 2) == 0
    end)
  end

  test "join with document post filter projections", %{db: client} do
    setup_collection(client, "A", Enum.map(0..19, &%{"x" => 2 * &1, "y" => &1}))
    Client.Index.ensure!(client, "A", :persistent, ["x", "y"])
    setup_collection(client, "B", Enum.map(0..19, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          FILTER doc1.y % 2 == 0
          RETURN [doc1.x, doc2.x]
    """

    %{
      "indexInfos" => [
        %{"projections" => first_proj, "filterProjections" => filter_proj},
        %{"projections" => second_proj}
      ]
    } = explain_join!(client, query)

    expect normalize(first_proj) == normalize(["x"])
    expect normalize(filter_proj) == normalize(["y"])
    expect normalize(second_proj) == normalize(["x"])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 5
    Enum.each(result, fn [a, b] -> expect a == b end)
  end

  test "join with document post filter filter projections", %{db: client} do
    setup_collection(
      client,
      "A",
      Enum.map(0..19, fn x -> %{"x" => 2 * x, "y" => %{"z" => %{"w" => x}}} end)
    )

    Client.Index.ensure!(client, "A", :persistent, ["x", "y.z"])
    setup_collection(client, "B", Enum.map(0..19, &%{"x" => &1}))
    Client.Index.ensure!(client, "B", :persistent, ["x"])

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc1.x == doc2.x
          FILTER doc1.y.z.w % 2 == 0
          RETURN [doc1, doc2.x]
    """

    %{
      "indexInfos" => [
        %{"projections" => [], "filterProjections" => filter_proj},
        %{"projections" => second_proj}
      ]
    } = explain_join!(client, query)

    expect normalize(filter_proj) == normalize([["y", "z", "w"]])
    expect normalize(second_proj) == normalize(["x"])

    result = Client.AQL.execute!(client, query, %{}, @query_options)
    expect length(result) == 5

    Enum.each(result, fn [%{"x" => x, "y" => %{"z" => %{"w" => w}}}, x2] ->
      expect x == x2
      expect rem(w, 2) == 0
    end)
  end

  test "join with primary index", %{db: client, is_cluster: is_cluster} do
    create_collection(client, is_cluster, "A", shard_keys: ["_key"])
    insert_docs(client, "A", Enum.map(0..19, &%{"_key" => "#{&1}"}))
    Client.Index.ensure!(client, "A", :persistent, ["x", "y"])

    doc_ids = Client.AQL.execute!(client, "FOR d IN A RETURN d._id")

    create_edge_collection(client, is_cluster, "B")
    edge_docs = Enum.map(doc_ids, fn id -> %{"_from" => id, "_to" => id} end)
    Client.Document.insert!(client, "B", edge_docs)

    for filter_attr <- ["_from", "_to"] do
      query = """
        FOR doc1 IN A
          SORT doc1._key
          FOR doc2 IN B
            FILTER doc1._key == doc2.#{filter_attr}
            RETURN [doc1, doc2.x]
      """

      {_nodes, join} = explain_join(client, query)
      refute join, "Expected no JoinNode for filter on #{filter_attr}"
    end
  end

  test "join with primary index projections", %{db: client, is_cluster: is_cluster} do
    create_collection(client, is_cluster, "A", shard_keys: ["_key"])
    insert_docs(client, "A", Enum.map(0..19, &%{"_key" => "#{&1}"}))
    Client.Index.ensure!(client, "A", :persistent, ["x", "y"])

    doc_ids = Client.AQL.execute!(client, "FOR d IN A RETURN d._id")

    create_edge_collection(client, is_cluster, "B")
    edge_docs = Enum.map(doc_ids, fn id -> %{"_from" => id, "_to" => id} end)
    Client.Document.insert!(client, "B", edge_docs)

    query = """
      FOR doc1 IN A
        SORT doc1._key
        FOR doc2 IN B
          FILTER doc1._key == doc2.x
          RETURN [doc1, doc2.x, doc1._id]
    """

    {_nodes, join} = explain_join(client, query)
    refute join, "Expected no JoinNode for _id projection"
  end

  test "join multiple joins", %{db: client, is_cluster: is_cluster} do
    create_collection(client, is_cluster, "A1", prototype: "prototype1")
    Client.Index.ensure!(client, "A1", :persistent, ["x"])
    create_collection(client, is_cluster, "B1", prototype: "prototype1")
    Client.Index.ensure!(client, "B1", :persistent, ["x"])
    create_collection(client, is_cluster, "A2", prototype: "prototype2")
    Client.Index.ensure!(client, "A2", :persistent, ["x"])
    create_collection(client, is_cluster, "B2", prototype: "prototype2")
    Client.Index.ensure!(client, "B2", :persistent, ["x"])

    query = """
      FOR doc1 IN A1
        FOR doc2 IN B1
          FILTER doc1.x == doc2.x

          FOR doc3 IN A2
          FOR doc4 IN B2
          FILTER doc3.x == doc4.x
          RETURN [doc1.x, doc2.x, doc3.x, doc4.x]
    """

    %{"plan" => %{"nodes" => nodes}} =
      Client.AQL.explain!(client, query, %{}, @query_options)

    joins = Enum.filter(nodes, &(&1["type"] == "JoinNode"))
    expect length(joins) == 2

    Enum.each(joins, fn join ->
      assert_projections(join, [["x"], ["x"]])
    end)
  end

  test "triple index join", %{db: client, is_cluster: is_cluster} do
    for name <- ~w(A B C) do
      create_collection(client, is_cluster, name)
      Client.Index.ensure!(client, name, :persistent, ["x"])
      insert_docs(client, name, Enum.map(0..19, &%{"x" => "#{&1}"}))
    end

    query = """
      FOR doc1 IN A
        FOR doc2 IN B
          FOR doc3 IN C
            FILTER doc1.x == doc2.x
            FILTER doc1.x == doc3.x
            RETURN [doc1.x, doc2.x, doc3.x]
    """

    join = explain_join!(client, query)
    expect length(join["indexInfos"]) == 3
    assert_projections(join, [["x"], ["x"], ["x"]])

    result = Client.AQL.execute!(client, query)
    expect length(result) == 20

    Enum.each(result, fn [a, b, c] ->
      expect a == b
      expect a == c
    end)
  end

  test "triple index join two of three A", %{db: client, is_cluster: is_cluster} do
    create_collection(client, is_cluster, "A")
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    insert_docs(client, "A", Enum.map(0..19, &%{"x" => "#{&1}"}))
    create_collection(client, is_cluster, "B")
    Client.Index.ensure!(client, "B", :persistent, ["x"])
    insert_docs(client, "B", Enum.map(0..19, &%{"x" => "#{&1}"}))
    create_collection(client, is_cluster, "C")
    Client.Index.ensure!(client, "C", :persistent, ["x", "y"])
    insert_docs(client, "C", Enum.map(0..19, &%{"y" => "#{&1}"}))

    query = """
      FOR doc1 IN A
        FOR doc2 IN B
          FOR doc3 IN C
            FILTER doc1.x == doc2.x
            FILTER doc1.x == doc3.y
            RETURN [doc1.x, doc2.x, doc3.y]
    """

    join = explain_join!(client, query)
    expect length(join["indexInfos"]) == 2
    assert_projections(join, [["x"], ["x"]])

    result = Client.AQL.execute!(client, query)
    expect length(result) == 20

    Enum.each(result, fn [a, b, c] ->
      expect a == b
      expect a == c
    end)
  end

  test "join past enumerate", %{db: client, is_cluster: is_cluster} do
    create_collection(client, is_cluster, "A")
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    insert_docs(client, "A", Enum.map(0..19, &%{"x" => "#{&1}"}))
    create_collection(client, is_cluster, "B")
    insert_docs(client, "B", Enum.map(0..19, &%{"x" => "#{&1}"}))
    create_collection(client, is_cluster, "C")
    Client.Index.ensure!(client, "C", :persistent, ["x"])
    insert_docs(client, "C", Enum.map(0..19, &%{"x" => "#{&1}"}))

    query = """
      FOR doc1 IN A
        FOR doc2 IN B
          FOR doc3 IN C
            FILTER doc1.x == doc2.x
            FILTER doc1.x == doc3.x
            RETURN [doc1.x, doc2.x, doc3.x]
    """

    join = explain_join!(client, query)
    expect length(join["indexInfos"]) == 2
    assert_projections(join, [["x"], ["x"]])

    result = Client.AQL.execute!(client, query)
    expect length(result) == 20

    Enum.each(result, fn [a, b, c] ->
      expect a == b
      expect a == c
    end)
  end

  test "two by two join", %{db: client, is_cluster: is_cluster} do
    for name <- ~w(A B C D) do
      create_collection(client, is_cluster, name)
      Client.Index.ensure!(client, name, :persistent, ["x"])
      insert_docs(client, name, Enum.map(0..19, &%{"x" => "#{&1}"}))
    end

    query = """
      FOR doc1 IN A
        FOR doc2 IN B
          FOR doc3 IN C
            FOR doc4 IN D
            FILTER doc1.x == doc3.x
            FILTER doc2.x == doc4.x
            RETURN [doc1.x, doc2.x, doc3.x, doc4.x]
    """

    join = explain_join!(client, query)
    expect length(join["indexInfos"]) == 2
    assert_projections(join, [["x"], ["x"]])

    result = Client.AQL.execute!(client, query)
    expect length(result) == 400

    Enum.each(result, fn [a, b, c, d] ->
      expect a == c
      expect b == d
    end)
  end

  test "join in cluster sharded local expansions", %{db: client} do
    Client.Collection.create!(client, "A",
      number_of_shards: 10,
      replication_factor: 1,
      write_concern: 1,
      wait_for_sync: true
    )

    insert_docs(client, "A", Enum.map(0..19, &%{"x" => &1}))

    query = "FOR c1 IN A FOR c2 IN A FILTER c1._key == c2._key RETURN c1._key"
    result = Client.AQL.execute!(client, query)
    count = Client.Collection.count!(client, "A")
    expect length(result) == count
  end

  test "join in cluster sharded local expansions materialize", %{db: client} do
    Client.Collection.create!(client, "A",
      number_of_shards: 10,
      replication_factor: 1,
      write_concern: 1,
      wait_for_sync: true
    )

    insert_docs(client, "A", Enum.map(0..9999, &%{"x" => &1}))

    query = "FOR c1 IN A FOR c2 IN A FILTER c1._key == c2._key RETURN c1"
    result = Client.AQL.execute!(client, query)
    count = Client.Collection.count!(client, "A")
    expect length(result) == count
  end

  test "late materialized", %{db: client, is_cluster: is_cluster} do
    create_collection(client, is_cluster, "A")
    Client.Index.ensure!(client, "A", :persistent, ["x"])
    insert_docs(client, "A", Enum.map(0..99, &%{"x" => "#{&1}"}))
    create_collection(client, is_cluster, "B")
    Client.Index.ensure!(client, "B", :persistent, ["x"])
    insert_docs(client, "B", Enum.map(0..99, &%{"x" => "#{&1}"}))

    query = """
      FOR doc1 IN A
        FOR doc2 IN B
          FILTER doc2.x == doc1.x
          SORT doc2.x
          LIMIT 20
          RETURN [doc1.x, doc2]
    """

    %{
      "indexInfos" => [
        _info0,
        %{
          "isLateMaterialized" => true,
          "producesOutput" => true,
          "indexCoversProjections" => true
        }
      ]
    } = join = explain_join!(client, query)

    assert_projections(join, [["x"], ["x"]])

    result = Client.AQL.execute!(client, query)
    expect length(result) == 20
    Enum.each(result, fn [x, %{"x" => x2}] -> expect x == x2 end)
  end

  test "late materialized push past join", %{db: client, is_cluster: is_cluster} do
    create_collection(client, is_cluster, "A")
    Client.Index.ensure!(client, "A", :persistent, ["x"], stored_values: ["z"])
    insert_docs(client, "A", Enum.map(0..99, &%{"x" => "#{&1}", "z" => 0}))
    create_collection(client, is_cluster, "B")
    Client.Index.ensure!(client, "B", :persistent, ["x"])
    insert_docs(client, "B", Enum.map(0..99, &%{"x" => "#{&1}"}))

    query = """
      FOR doc1 IN A
        SORT doc1.x
        FOR doc2 IN B
          FILTER doc2.x == doc1.x
          SORT doc2.x
          LIMIT 20
          FILTER doc1.z == 0
          RETURN [doc1, doc2]
    """

    {nodes, join} = explain_join(client, query)

    assert %{
             "indexInfos" => [
               %{
                 "isLateMaterialized" => true,
                 "producesOutput" => true,
                 "indexCoversProjections" => true,
                 "projections" => first_proj
               },
               %{
                 "isLateMaterialized" => true,
                 "producesOutput" => true,
                 "indexCoversProjections" => true,
                 "projections" => second_proj
               }
             ]
           } = join

    expect normalize(first_proj) == [["z"]]
    expect normalize(second_proj) == [["x"]]

    node_types =
      nodes
      |> Enum.map(& &1["type"])
      |> Enum.filter(&(&1 in ~w(LimitNode MaterializeNode FilterNode RemoteNode)))

    if is_cluster do
      expect node_types ==
               ~w(LimitNode MaterializeNode MaterializeNode RemoteNode LimitNode FilterNode)
    else
      expect node_types == ~w(LimitNode FilterNode MaterializeNode MaterializeNode)
    end

    result = Client.AQL.execute!(client, query)
    expect length(result) == 20
    Enum.each(result, fn [%{"x" => x}, %{"x" => y}] -> expect x == y end)
  end

  defp setup_unique_stream_collections(client, is_cluster) do
    create_collection(client, is_cluster, "A")
    Client.Index.ensure!(client, "A", :persistent, ["y", "z", "x"], unique: true)
    insert_docs(client, "A", Enum.map(0..99, &%{"x" => &1, "y" => &1, "z" => &1}))

    create_collection(client, is_cluster, "C", shard_keys: ["z"])
    Client.Index.ensure!(client, "C", :persistent, ["y", "z", "x"], unique: true)
    insert_docs(client, "C", Enum.map(0..99, &%{"x" => &1, "y" => &1, "z" => &1}))

    create_collection(client, is_cluster, "B")
    Client.Index.ensure!(client, "B", :persistent, ["x"], unique: true)
    insert_docs(client, "B", Enum.map(0..99, &%{"x" => &1}))
  end

  test "unique stream - both sides unique", %{db: client, is_cluster: is_cluster} do
    setup_unique_stream_collections(client, is_cluster)

    query = """
      FOR a IN A
        FOR b IN B
          FILTER a.z == 12 && a.y == 12 && b.x == a.x
          RETURN [a, b]
    """

    assert %{
             "indexInfos" => [
               %{"isUniqueStream" => true},
               %{"isUniqueStream" => true}
             ]
           } = explain_join!(client, query, %{})

    expect [[%{"z" => 12}, %{"x" => 12}]] = Client.AQL.execute!(client, query)
  end

  test "unique stream - only second side unique", %{db: client, is_cluster: is_cluster} do
    setup_unique_stream_collections(client, is_cluster)

    query = """
      FOR a IN C
        FOR b IN B
          FILTER a.y == 12 && b.x == a.z
          RETURN [a, b]
    """

    assert %{
             "indexInfos" => [
               first_info,
               %{"isUniqueStream" => true}
             ]
           } = explain_join!(client, query, %{})

    refute first_info["isUniqueStream"]
    expect [[%{"z" => 12}, %{"x" => 12}]] = Client.AQL.execute!(client, query)
  end

  test "unique stream - no matching results", %{db: client, is_cluster: is_cluster} do
    setup_unique_stream_collections(client, is_cluster)

    query = """
      FOR a IN C
        FOR b IN B
          FILTER a.y == "DOES NOT EXIST" && b.x == a.z
          RETURN [a, b]
    """

    assert %{
             "indexInfos" => [
               first_info,
               %{"isUniqueStream" => true}
             ]
           } = explain_join!(client, query, %{})

    refute first_info["isUniqueStream"]
    expect [] = Client.AQL.execute!(client, query)
  end
end
