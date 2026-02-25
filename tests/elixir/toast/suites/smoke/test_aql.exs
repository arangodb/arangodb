defmodule Smoke.AqlTest do
  use Smoke.Suite

  test "simple return", %{client: client} do
    assert {:ok, [1]} = Client.AQL.execute(client, "RETURN 1")
  end

  test "with bind variables", %{client: client} do
    assert {:ok, [42]} = Client.AQL.execute(client, "RETURN @val", %{"val" => 42})
  end

  test "multi-row result", %{client: client} do
    assert {:ok, [1, 2, 3]} = Client.AQL.execute(client, "FOR i IN 1..3 RETURN i")
  end

  test "bang variant returns bare results", %{client: client} do
    assert [1] = Client.AQL.execute!(client, "RETURN 1")
  end

  test "invalid query returns error", %{client: client} do
    assert {:error, %{status: 400}} = Client.AQL.execute(client, "INVALID SYNTAX HERE")
  end
end
