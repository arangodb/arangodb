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
