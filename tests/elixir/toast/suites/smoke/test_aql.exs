defmodule SmokeTest.AqlTest do
  use Toast.TestCase

  test "simple return", %{client: client} do
    assert {:ok, [1]} = Client.aql(client, "RETURN 1")
  end

  test "with bind variables", %{client: client} do
    assert {:ok, [42]} = Client.aql(client, "RETURN @val", %{"val" => 42})
  end

  test "multi-row result", %{client: client} do
    assert {:ok, [1, 2, 3]} = Client.aql(client, "FOR i IN 1..3 RETURN i")
  end

  test "bang variant returns bare results", %{client: client} do
    assert [1] = Client.aql!(client, "RETURN 1")
  end

  test "invalid query returns error", %{client: client} do
    assert {:error, %{status: 400}} = Client.aql(client, "INVALID SYNTAX HERE")
  end
end
