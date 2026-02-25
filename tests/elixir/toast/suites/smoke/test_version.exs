defmodule Smoke.VersionTest do
  use Smoke.Suite

  test "returns arango server info", %{client: client} do
    assert {:ok, body} = Client.Admin.version(client)
    assert body["server"] == "arango"
    assert is_binary(body["version"])
  end

  test "endpoint is accessible via raw HTTP", %{endpoint: endpoint} do
    assert {:ok, %{status: 200}} = Req.get(endpoint <> "/_api/version", retry: false)
  end
end
