defmodule SmokeTest.CollectionTest do
  use Toast.TestCase

  setup %{client: client} do
    name = "test_coll_#{System.unique_integer([:positive])}"
    on_exit(fn -> Client.drop_collection(client, name) end)
    %{collection: name}
  end

  test "create and list collection", %{client: client, collection: name} do
    assert {:ok, _} = Client.create_collection(client, name)
    assert {:ok, collections} = Client.list_collections(client, exclude_system: true)
    assert Enum.any?(collections, &(&1["name"] == name))
  end

  test "drop collection", %{client: client, collection: name} do
    assert {:ok, _} = Client.create_collection(client, name)
    assert :ok = Client.drop_collection(client, name)
    assert {:ok, collections} = Client.list_collections(client, exclude_system: true)
    refute Enum.any?(collections, &(&1["name"] == name))
  end

  test "insert and get document", %{client: client, collection: name} do
    assert {:ok, _} = Client.create_collection(client, name)
    assert {:ok, meta} = Client.insert_document(client, name, %{"value" => 42})
    assert {:ok, doc} = Client.get_document(client, name, meta["_key"])
    assert doc["value"] == 42
  end

  test "insert and remove document", %{client: client, collection: name} do
    assert {:ok, _} = Client.create_collection(client, name)
    assert {:ok, meta} = Client.insert_document(client, name, %{"hello" => "world"})
    assert :ok = Client.remove_document(client, name, meta["_key"])
    assert {:error, %{status: 404}} = Client.get_document(client, name, meta["_key"])
  end
end
