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

defmodule Smoke.ActivitiesTest do
  use Smoke.Suite

  @tag :cluster_only
  test "gets activities from all servers", %{client: client} do
    {:ok, %{"Health" => health}} = Client.Admin.Cluster.health(client)
    {:ok, %{"activities_per_server" => activities_per_server}} = Client.Admin.Activities.all(client)
    assert Enum.sort(health |> Map.keys()) == Enum.sort(activities_per_server |> Map.keys())
  end
end
