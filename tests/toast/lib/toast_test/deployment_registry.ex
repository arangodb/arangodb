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

defmodule ToastTest.DeploymentRegistry do
  @moduledoc "Registry mapping suite modules to their active deployments."

  use Agent

  def start_link(_opts \\ []) do
    Agent.start_link(fn -> %{deployments: %{}, extra_contexts: %{}} end, name: __MODULE__)
  end

  @spec clear() :: :ok
  def clear do
    Agent.update(__MODULE__, fn _ -> %{deployments: %{}, extra_contexts: %{}} end)
  end

  @spec put(module(), Toast.Deployment.t()) :: :ok
  def put(suite_module, deployment) do
    Agent.update(__MODULE__, &put_in(&1, [:deployments, suite_module], deployment))
  end

  @spec fetch(module()) :: {:ok, Toast.Deployment.t()} | :error
  def fetch(suite_module) do
    case Agent.get(__MODULE__, &get_in(&1, [:deployments, suite_module])) do
      nil -> :error
      deployment -> {:ok, deployment}
    end
  end

  @spec get(module()) :: Toast.Deployment.t()
  def get(suite_module) do
    case fetch(suite_module) do
      {:ok, deployment} -> deployment
      :error -> raise "No deployment registered for suite #{inspect(suite_module)}"
    end
  end

  @spec put_extra_context(module(), map()) :: :ok
  def put_extra_context(suite_module, extra_context) do
    Agent.update(__MODULE__, &put_in(&1, [:extra_contexts, suite_module], extra_context))
  end

  @spec get_extra_context(module()) :: map()
  def get_extra_context(suite_module) do
    Agent.get(__MODULE__, &Map.get(&1.extra_contexts, suite_module, %{}))
  end
end
