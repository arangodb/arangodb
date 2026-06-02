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

defmodule ToastTest.Interactive do
  @moduledoc """
  Interactive debugging helper for running tests or complete suites against a
  manually-started deployment, with proper ExUnit lifecycle support
  (setup_all, setup, on_exit).

  Accepts a file path, directory path, test module, or suite module — the
  target type is inferred automatically. All files in the suite directory are
  always compiled so that cross-module dependencies are available.

  ## Examples

      # Run a single test file
      Interactive.run("suites/smoke/test_version.exs", deployment: deployment)

      # Run a single test module (compiles suite directory automatically)
      Interactive.run(Smoke.VersionTest, deployment: deployment)

      # Run a complete suite by directory
      Interactive.run("suites/smoke/", deployment: deployment)

      # Run a complete suite by module
      Interactive.run(Smoke.Suite, deployment: deployment)

      # Filter by test name
      Interactive.run(Smoke.Suite, deployment: deployment, test: "version")

  ## Limitations vs `mix toast`

  - No event pipeline, result collection, or CI packaging
  - No between-tests health checks (crashes won't auto-abort the suite)
  - No timeout enforcement
  - No test bucketing or tag-based filtering
  """

  alias __MODULE__.Resolver
  alias __MODULE__.TestRunner

  @spec run(module() | String.t(), keyword()) :: [map()]
  def run(target, opts \\ []) do
    ensure_ex_unit_started()
    {suite_module, test_modules} = Resolver.resolve(target)
    do_run(suite_module, test_modules, opts)
  end

  defp do_run(suite_module, test_modules, opts) do
    deployment = Keyword.fetch!(opts, :deployment)
    test_name = Keyword.get(opts, :test)
    exclude_tags = mode_exclusions(deployment)

    validate_deployment!(suite_module, deployment)
    ToastTest.DeploymentRegistry.put(suite_module, deployment)
    run_setup(suite_module, deployment)

    results =
      try do
        Enum.flat_map(test_modules, fn module ->
          if length(test_modules) > 1, do: IO.puts("\n── #{inspect(module)} ──")
          results = TestRunner.run_module_tests(module, test_name, exclude_tags)
          print_summary(results)
          results
        end)
      after
        run_teardown(suite_module, deployment)
      end

    if length(test_modules) > 1, do: print_summary(results, "Suite total")
    results
  end

  defp mode_exclusions(%Toast.Deployment{} = deployment) do
    if Toast.Deployment.cluster?(deployment),
      do: [:single_only],
      else: [:cluster_only]
  end

  defp mode_exclusions(_), do: []

  defp run_setup(suite_module, deployment) do
    extra_context =
      if function_exported?(suite_module, :setup_deployment, 1) do
        case suite_module.setup_deployment(deployment) do
          {:ok, ctx} -> ctx
          {:error, reason} -> raise "setup_deployment failed: #{inspect(reason)}"
        end
      else
        %{}
      end

    ToastTest.DeploymentRegistry.put_extra_context(suite_module, extra_context)
  end

  defp run_teardown(suite_module, deployment) do
    if function_exported?(suite_module, :teardown_deployment, 1) do
      suite_module.teardown_deployment(deployment)
    end
  end

  defp validate_deployment!(suite_module, deployment) do
    suite_config = suite_module.deployment_config()

    case Keyword.get(suite_config, :mode, :auto) do
      mode when mode in [:auto, :manual] ->
        :ok

      :cluster ->
        validate_mode!(:cluster, deployment)
        validate_topology!(suite_config, deployment)

      mode ->
        validate_mode!(mode, deployment)
    end
  end

  defp validate_mode!(expected_mode, deployment) do
    is_cluster = Toast.Deployment.cluster?(deployment)

    actual_mode =
      if is_cluster, do: :cluster, else: :single_server

    if expected_mode != actual_mode do
      raise ArgumentError,
            "suite requires #{expected_mode}, got #{actual_mode}"
    end
  end

  defp validate_topology!(suite_config, deployment) do
    check_server_count(deployment, :coordinator, Keyword.get(suite_config, :cluster_coordinators))
    check_server_count(deployment, :dbserver, Keyword.get(suite_config, :cluster_dbservers))
    check_server_count(deployment, :agent, Keyword.get(suite_config, :cluster_agents))
  end

  defp check_server_count(_deployment, _role, nil), do: :ok

  defp check_server_count(deployment, role, required) do
    actual = length(Toast.Deployment.servers(deployment, role: role))

    if actual < required do
      raise ArgumentError,
            "suite requires #{required} #{role}(s), deployment has #{actual}"
    end
  end

  defp print_summary(results, label \\ nil) do
    passed = Enum.count(results, &(&1.outcome == :passed))
    failed = Enum.count(results, &(&1.outcome == :failed))
    prefix = if label, do: "\n#{label}: ", else: ""
    IO.puts("#{prefix}#{passed} passed, #{failed} failed")
  end

  defp ensure_ex_unit_started do
    case Application.ensure_all_started(:ex_unit) do
      {:ok, started} when started != [] -> ExUnit.start(autorun: false)
      {:ok, _} -> :ok
    end
  end
end
