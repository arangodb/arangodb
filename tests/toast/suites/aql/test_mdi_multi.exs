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

defmodule Aql.MdiMultiTest do
  use Aql.Suite

  @collection "toast_mdi_multi"

  @op_cases ~w(none eq le ge le2 ge2 lt gt legt)a

  setup_all %{client: client} do
    Client.Collection.create!(client, @collection)
    on_exit(fn -> Client.Collection.drop(client, @collection) end)

    Client.Index.ensure!(client, @collection, :mdi, ["x", "y", "z", "a.w"],
      field_value_types: "double",
      name: "mdiIndex"
    )

    query = """
    FOR x IN 0..10
      FOR y IN 0..10
        FOR z IN 0..10
          FOR w IN 0..10
            INSERT {x, y, z, a: {w}} INTO #{@collection}
    """

    Client.AQL.execute!(client, query)
    :ok
  end

  defp condition(:none, _name), do: "true"
  defp condition(:eq, name), do: "#{name} == 5"
  defp condition(:le, name), do: "#{name} <= 5"
  defp condition(:ge, name), do: "#{name} >= 5"
  defp condition(:ge2, name), do: "#{name} >= 5 && #{name} <= 6"
  defp condition(:le2, name), do: "#{name} <= 5 && #{name} >= 4"
  defp condition(:lt, name), do: "#{name} < 5"
  defp condition(:gt, name), do: "#{name} > 5"
  defp condition(:legt, name), do: "#{name} <= 5 && #{name} > 4"

  defp result_set(:none), do: Enum.to_list(0..10)
  defp result_set(:eq), do: [5]
  defp result_set(:le), do: Enum.to_list(0..5)
  defp result_set(:ge), do: Enum.to_list(5..10)
  defp result_set(:ge2), do: [5, 6]
  defp result_set(:le2), do: [4, 5]
  defp result_set(:lt), do: Enum.to_list(0..4)
  defp result_set(:gt), do: Enum.to_list(6..10)
  defp result_set(:legt), do: [5]

  defp expected_results(op_x, op_y, op_z, op_w) do
    for x <- result_set(op_x),
        y <- result_set(op_y),
        z <- result_set(op_z),
        w <- result_set(op_w),
        do: %{"x" => x, "y" => y, "z" => z, "w" => w}
  end

  defp build_query(op_x, op_y, op_z, op_w) do
    """
    FOR d IN #{@collection}
      FILTER #{condition(op_x, "d.x")}
      FILTER #{condition(op_y, "d.y")}
      FILTER #{condition(op_z, "d.z")}
      FILTER #{condition(op_w, "d.a.w")}
      RETURN {x: d.x, y: d.y, z: d.z, w: d.a.w}
    """
  end

  defp assert_uses_mdi_index(client, query, ops) do
    %{"plan" => %{"rules" => rules, "nodes" => nodes}} =
      Client.AQL.explain!(client, query)

    node_types =
      nodes
      |> Enum.map(& &1["type"])
      |> Enum.reject(&(&1 in ["GatherNode", "RemoteNode"]))

    expect node_types == ["SingletonNode", "IndexNode", "CalculationNode", "ReturnNode"]
    expect "use-indexes" in rules

    has_strict_bound = Enum.any?(ops, &(&1 in [:lt, :gt, :legt]))

    unless has_strict_bound do
      expect "remove-filter-covered-by-index" in rules
    end
  end

  for op_x <- [:none, :eq],
      op_y <- [:none, :le, :gt],
      op_z <- @op_cases,
      op_w <- @op_cases,
      {op_x, op_y, op_z, op_w} != {:none, :none, :none, :none} do
    test_name = "mdi index #{op_x}_#{op_y}_#{op_z}_#{op_w}"

    @op_x op_x
    @op_y op_y
    @op_z op_z
    @op_w op_w

    test test_name, %{client: client} do
      query = build_query(@op_x, @op_y, @op_z, @op_w)
      assert_uses_mdi_index(client, query, [@op_x, @op_y, @op_z, @op_w])

      result = Client.AQL.execute!(client, query) |> Enum.sort()
      expected = expected_results(@op_x, @op_y, @op_z, @op_w) |> Enum.sort()
      assert expected == result
    end
  end
end
