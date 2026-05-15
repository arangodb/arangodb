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

	def assert_array_larger_than(array, length) do
		assert length(array) > length, "Failed: #{length(array)} > #{length}: #{Jason.encode!(array)}"
	end

	defp activity_rest_handler?(activity) do
		activity["type"] == "RestHandler" and
			get_in(activity, ["data", "handler"]) == "ActivityRegistryRestHandler"
	end

	test "activities include at least current activity rest handler", %{client: client} do
		{:ok, %{"activities" => activities}} = Client.Admin.Activities.get(client)
		assert_array_larger_than(Enum.filter(activities, &activity_rest_handler?/1), 0)
	end

	@tag :cluster_only
	test "gets activities from each server", %{client: client, deployment: deployment} do
		{:ok, %{"Health" => health}} = Client.Admin.Cluster.health(client)
		health
		|> Enum.reject(fn {_id, props} -> props["Role"] == "Agent" end)
		|> Enum.each(fn {server_id, _props} ->
			{:ok, server_client} = Toast.Deployment.client_for_arango_id(deployment, server_id)
			{:ok, %{"activities" => activities}} = Client.Admin.Activities.get(server_client)
			assert_array_larger_than(activities, 0)
			assert_array_larger_than(Enum.filter(activities, &activity_rest_handler?/1), 0)
		end)
	end
	
  @tag :cluster_only
  test "gets activities from all servers", %{client: client} do
    {:ok, %{"Health" => health}} = Client.Admin.Cluster.health(client)
    {:ok, %{"activities_per_server" => activities_per_server}} = Client.Admin.Activities.all(client)
    assert Enum.sort(health |> Map.keys()) == Enum.sort(activities_per_server |> Map.keys())
  end

	@tag :single_only
	test "all servers endpoint is not available on single server", %{client: client} do
		assert {:error, %{status: 403}} = Client.Admin.Activities.all(client)
	end

	@tag :cluster_only
	test "all servers each include at least one activity rest handler", %{client: client} do
		{:ok, %{"activities_per_server" => activities_per_server}} =
			Client.Admin.Activities.all(client)

		Enum.each(activities_per_server, fn {server_id, activities} ->
			rest_handler_activities = Enum.filter(activities, &activity_rest_handler?/1)
			assert length(rest_handler_activities) > 0,
				"Failed for server #{server_id}: #{length(rest_handler_activities)} > 0, #{Jason.encode!(rest_handler_activities)}"
		end)
	end

	# ERROR_CLUSTER_TIMEOUT         = 1457 (agency does not yet know about the failed server)
	# ERROR_CLUSTER_CONNECTION_LOST = 1465 (agency knows about the failed server)
	@cluster_unreachable_errors [1457, 1465]

	@tag :cluster_only
	test "all servers endpoint is ok if one server is not reached",
			 %{client: client, deployment: deployment} do
		{:ok, %{"Health" => health}} = Client.Admin.Cluster.health(client)

		{suspending_dbserver, _props} =
			Enum.find(health, fn {_id, props} -> props["Role"] == "DBServer" end)

		:ok = Toast.Deployment.pause_server(deployment, arango_id: suspending_dbserver)
		on_exit(fn -> Toast.Deployment.resume_server(deployment, arango_id: suspending_dbserver) end)

		{:ok, %{"activities_per_server" => activities_per_server}} =
			Client.Admin.Activities.all(client)

		entry_from_suspended_server = activities_per_server[suspending_dbserver]

		assert entry_from_suspended_server,
			"expected an entry for paused server #{suspending_dbserver}; got keys: #{inspect(Map.keys(activities_per_server))}"

		assert entry_from_suspended_server["number"] in @cluster_unreachable_errors,
			"expected one of #{inspect(@cluster_unreachable_errors)}, got #{inspect(entry_from_suspended_server)}"
	end
end
