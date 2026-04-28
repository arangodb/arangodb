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

defmodule ToastTest.DeploymentRegistryTest do
  use ExUnit.Case, async: false

  import Toast.DeploymentTestHelpers, only: [setup_deployment_registry: 1]

  alias ToastTest.DeploymentRegistry

  setup :setup_deployment_registry

  test "put and get deployment by suite module" do
    deployment = %{endpoint: "http://localhost:8529", id: "test-1"}
    DeploymentRegistry.put(MyApp.Suite, deployment)
    assert DeploymentRegistry.get(MyApp.Suite) == deployment
  end

  test "get raises when no deployment registered" do
    assert_raise RuntimeError, ~r/No deployment registered/, fn ->
      DeploymentRegistry.get(NonExistent.Suite)
    end
  end

  test "clear removes all entries" do
    DeploymentRegistry.put(Suite1, %{id: "1"})
    DeploymentRegistry.put(Suite2, %{id: "2"})
    DeploymentRegistry.clear()
    assert_raise RuntimeError, fn -> DeploymentRegistry.get(Suite1) end
    assert_raise RuntimeError, fn -> DeploymentRegistry.get(Suite2) end
  end

  test "put overwrites existing entry" do
    DeploymentRegistry.put(MySuite, %{id: "old"})
    DeploymentRegistry.put(MySuite, %{id: "new"})
    assert DeploymentRegistry.get(MySuite) == %{id: "new"}
  end

  test "fetch returns {:ok, deployment} when registered" do
    deployment = %{endpoint: "http://localhost:8529", id: "fetch-test"}
    DeploymentRegistry.put(FetchSuite, deployment)
    assert DeploymentRegistry.fetch(FetchSuite) == {:ok, deployment}
  end

  test "fetch returns :error when not registered" do
    assert DeploymentRegistry.fetch(UnregisteredSuite) == :error
  end
end
