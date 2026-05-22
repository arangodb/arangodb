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

defmodule ToastTest.InteractiveTest do
  use ExUnit.Case, async: false

  import ExUnit.CaptureIO

  import Toast.DeploymentTestHelpers, only: [setup_deployment_registry: 1]

  alias ToastTest.DeploymentRegistry
  alias ToastTest.Interactive

  defmodule Fixtures.SimpleSuite do
    use ToastTest.Suite
  end

  defmodule Fixtures.SuiteTestA do
    use ExUnit.Case, async: false
    def __toast_suite__, do: Fixtures.SimpleSuite
    test "alpha", do: assert(true)
  end

  defmodule Fixtures.SuiteTestB do
    use ExUnit.Case, async: false
    def __toast_suite__, do: Fixtures.SimpleSuite
    test "beta", do: assert(true)
    test "gamma", do: assert(true)
  end

  defmodule Fixtures.SuiteWithCallbacks do
    use ToastTest.Suite

    @impl ToastTest.Suite
    def setup_deployment(_deployment) do
      collector = Process.whereis(:interactive_test_collector)
      if collector, do: send(collector, :setup_deployment_called)
      {:ok, %{from_setup: true}}
    end

    @impl ToastTest.Suite
    def teardown_deployment(_deployment) do
      collector = Process.whereis(:interactive_test_collector)
      if collector, do: send(collector, :teardown_deployment_called)
      :ok
    end
  end

  defmodule Fixtures.CallbackSuiteTest do
    use ExUnit.Case, async: false
    def __toast_suite__, do: Fixtures.SuiteWithCallbacks
    test "ok", do: assert(true)
  end

  defmodule Fixtures.SuiteWithFailingSetup do
    use ToastTest.Suite

    @impl ToastTest.Suite
    def setup_deployment(_deployment) do
      collector = Process.whereis(:interactive_test_collector)

      if collector do
        {:error, "setup exploded"}
      else
        {:ok, %{}}
      end
    end
  end

  defmodule Fixtures.FailingSetupSuiteTest do
    use ExUnit.Case, async: false
    def __toast_suite__, do: Fixtures.SuiteWithFailingSetup
    test "will not run", do: assert(true)
  end

  defmodule Fixtures.TeardownOnFailureSuite do
    use ToastTest.Suite

    @impl ToastTest.Suite
    def teardown_deployment(_deployment) do
      collector = Process.whereis(:interactive_test_collector)
      if collector, do: send(collector, :teardown_ran_after_failure)
      :ok
    end
  end

  defmodule Fixtures.TeardownOnFailureTest do
    use ExUnit.Case, async: false
    def __toast_suite__, do: Fixtures.TeardownOnFailureSuite

    test "fails" do
      if Process.whereis(:interactive_test_collector),
        do: raise("boom")
    end
  end

  defmodule Fixtures.ClusterSuite do
    use ToastTest.Suite, mode: :cluster, cluster_coordinators: 2
  end

  defmodule Fixtures.ClusterSuiteTest do
    use ExUnit.Case, async: false
    def __toast_suite__, do: Fixtures.ClusterSuite
    test "cluster test", do: assert(true)
  end

  defmodule Fixtures.SingleSuite do
    use ToastTest.Suite, mode: :single_server
  end

  defmodule Fixtures.SingleSuiteTest do
    use ExUnit.Case, async: false
    def __toast_suite__, do: Fixtures.SingleSuite
    test "single test", do: assert(true)
  end

  setup :setup_deployment_registry

  defp register_collector do
    Process.register(self(), :interactive_test_collector)

    on_exit(fn ->
      try do
        Process.unregister(:interactive_test_collector)
      rescue
        _ -> :ok
      end
    end)
  end

  describe "summary output" do
    test "prints N passed, M failed" do
      defmodule SummaryTestSuite do
        use ToastTest.Suite
      end

      defmodule SummaryModule do
        use ExUnit.Case, async: false
        def __toast_suite__, do: SummaryTestSuite

        setup_all do
          {:ok, %{}}
        end

        test "ok", do: assert(true)

        test "fail" do
          if Process.whereis(:interactive_test_collector), do: raise("boom")
        end
      end

      register_collector()

      output =
        capture_io(fn ->
          Interactive.run(SummaryModule, deployment: :fake)
        end)

      assert output =~ "1 passed, 1 failed"
    end

    test "prints summary for single test module" do
      output =
        capture_io(fn ->
          Interactive.run(Fixtures.SuiteTestA, deployment: :fake)
        end)

      assert output =~ "1 passed, 0 failed"
    end
  end

  describe "deployment registration" do
    test "registers deployment under suite module" do
      results = run_capturing_io(Fixtures.SuiteTestA, deployment: :suite_deploy)
      assert [%{outcome: :passed}] = results
      assert DeploymentRegistry.get(Fixtures.SimpleSuite) == :suite_deploy
    end

    test "raises when :deployment option is missing" do
      assert_raise KeyError, fn ->
        Interactive.run(Fixtures.SuiteTestA, [])
      end
    end
  end

  describe "deployment validation" do
    test "accepts compatible cluster deployment for cluster suite" do
      deployment = make_cluster_deployment(coordinators: 2, dbservers: 3, agents: 3)

      results = run_capturing_io(Fixtures.ClusterSuite, deployment: deployment)
      assert Enum.all?(results, &(&1.outcome == :passed))
    end

    test "rejects single-server deployment for cluster suite" do
      deployment = make_single_deployment()

      assert_raise ArgumentError, ~r/requires cluster.*got single/, fn ->
        run_capturing_io(Fixtures.ClusterSuite, deployment: deployment)
      end
    end

    test "rejects cluster deployment for single-server suite" do
      deployment = make_cluster_deployment()

      assert_raise ArgumentError, ~r/requires single_server.*got cluster/, fn ->
        run_capturing_io(Fixtures.SingleSuite, deployment: deployment)
      end
    end

    test "rejects cluster with insufficient coordinators" do
      deployment = make_cluster_deployment(coordinators: 1)

      assert_raise ArgumentError, ~r/coordinator/, fn ->
        run_capturing_io(Fixtures.ClusterSuite, deployment: deployment)
      end
    end

    test "accepts auto-mode suite with any deployment" do
      single = make_single_deployment()
      cluster = make_cluster_deployment()

      results = run_capturing_io(Fixtures.SimpleSuite, deployment: single)
      assert Enum.all?(results, &(&1.outcome == :passed))

      results = run_capturing_io(Fixtures.SimpleSuite, deployment: cluster)
      assert Enum.all?(results, &(&1.outcome == :passed))
    end
  end

  describe "run/2 suite mode with module" do
    test "runs all test modules belonging to the suite" do
      results = run_capturing_io(Fixtures.SimpleSuite, deployment: :fake)
      names = Enum.map(results, & &1.name) |> Enum.sort()
      assert :"test alpha" in names
      assert :"test beta" in names
      assert :"test gamma" in names
      assert Enum.all?(results, &(&1.outcome == :passed))
    end

    test "calls setup_deployment and teardown_deployment" do
      register_collector()

      capture_io(fn ->
        Interactive.run(Fixtures.SuiteWithCallbacks, deployment: :fake)
      end)

      assert_received :setup_deployment_called
      assert_received :teardown_deployment_called
    end

    test "resolves suite and runs lifecycle when passing a test module" do
      register_collector()

      capture_io(fn ->
        Interactive.run(Fixtures.CallbackSuiteTest, deployment: :fake)
      end)

      assert_received :setup_deployment_called
      assert_received :teardown_deployment_called
    end

    test "extra context from setup_deployment is stored" do
      register_collector()

      capture_io(fn ->
        Interactive.run(Fixtures.SuiteWithCallbacks, deployment: :fake)
      end)

      assert DeploymentRegistry.get_extra_context(Fixtures.SuiteWithCallbacks) == %{
               from_setup: true
             }
    end

    test "teardown runs even when a test fails" do
      register_collector()

      capture_io(fn ->
        Interactive.run(Fixtures.TeardownOnFailureSuite, deployment: :fake)
      end)

      assert_received :teardown_ran_after_failure
    end

    test "setup_deployment failure raises" do
      register_collector()

      assert_raise RuntimeError, ~r/setup_deployment failed/, fn ->
        capture_io(fn ->
          Interactive.run(Fixtures.SuiteWithFailingSetup, deployment: :fake)
        end)
      end
    end

    test "filters by test name across all modules" do
      results = run_capturing_io(Fixtures.SimpleSuite, deployment: :fake, test: "alpha")
      assert length(results) == 1
      assert [%{name: :"test alpha"}] = results
    end

    test "raises when module is not loaded and no suite directory found" do
      assert_raise ArgumentError, ~r/is not loaded or does not belong to a suite/, fn ->
        Interactive.run(Nonexistent.Suite, deployment: :fake)
      end
    end

    test "registers deployment under suite key" do
      capture_io(fn ->
        Interactive.run(Fixtures.SimpleSuite, deployment: :suite_deploy)
      end)

      assert DeploymentRegistry.get(Fixtures.SimpleSuite) == :suite_deploy
    end

    test "prints per-module headers and aggregate summary" do
      output =
        capture_io(fn ->
          Interactive.run(Fixtures.SimpleSuite, deployment: :fake)
        end)

      assert output =~ "SuiteTestA"
      assert output =~ "SuiteTestB"
      assert output =~ "Suite total:"
    end
  end

  describe "run/2 suite mode with path" do
    @tag :tmp_dir
    test "compiles and runs suite from directory", %{tmp_dir: tmp_dir} do
      {suite_dir, _uid} = create_suite_fixture(tmp_dir)
      results = run_capturing_io(suite_dir, deployment: :fake)
      assert length(results) == 1
      assert [%{outcome: :passed}] = results
    end

    @tag :tmp_dir
    test "raises when no suite.ex found in directory or parents", %{tmp_dir: tmp_dir} do
      assert_raise ArgumentError, ~r/no suite\.ex found/, fn ->
        Interactive.run(tmp_dir, deployment: :fake)
      end
    end

    @tag :tmp_dir
    test "subfolder runs only tests in that subfolder", %{tmp_dir: tmp_dir} do
      {suite_dir, uid} = create_suite_fixture(tmp_dir, subfolders: true)
      sub_dir = Path.join(suite_dir, "sub")
      results = run_capturing_io(sub_dir, deployment: :fake)
      assert length(results) == 1
      assert [%{outcome: :passed}] = results
      assert hd(results).module == :"Elixir.TmpSuite#{uid}.SubTest"
    end

    @tag :tmp_dir
    test "suite root includes tests from subfolders", %{tmp_dir: tmp_dir} do
      {suite_dir, _uid} = create_suite_fixture(tmp_dir, subfolders: true)
      results = run_capturing_io(suite_dir, deployment: :fake)
      assert length(results) == 2
      assert Enum.all?(results, &(&1.outcome == :passed))
    end

    @tag :tmp_dir
    test "file in subfolder compiles and runs correctly", %{tmp_dir: tmp_dir} do
      {suite_dir, uid} = create_suite_fixture(tmp_dir, subfolders: true)
      file = Path.join([suite_dir, "sub", "test_sub.exs"])
      results = run_capturing_io(file, deployment: :fake)
      assert length(results) == 1
      assert hd(results).module == :"Elixir.TmpSuite#{uid}.SubTest"
    end
  end

  defp run_capturing_io(target, opts) do
    capture_io(fn ->
      send(self(), {:interactive_results, Interactive.run(target, opts)})
    end)

    assert_received {:interactive_results, results}
    results
  end

  defp create_suite_fixture(base_dir, opts \\ []) do
    suite_dir = Path.join(base_dir, "test_suite_#{System.unique_integer([:positive])}")
    File.mkdir_p!(suite_dir)

    uid = System.unique_integer([:positive])

    File.write!(Path.join(suite_dir, "suite.ex"), """
    defmodule TmpSuite#{uid}.Suite do
      use ToastTest.Suite
    end
    """)

    File.write!(Path.join(suite_dir, "test_example.exs"), """
    defmodule TmpSuite#{uid}.ExampleTest do
      use ExUnit.Case, async: false
      def __toast_suite__, do: TmpSuite#{uid}.Suite
      test "it works", do: assert(true)
    end
    """)

    if opts[:subfolders] do
      sub_dir = Path.join(suite_dir, "sub")
      File.mkdir_p!(sub_dir)

      File.write!(Path.join(sub_dir, "test_sub.exs"), """
      defmodule TmpSuite#{uid}.SubTest do
        use ExUnit.Case, async: false
        def __toast_suite__, do: TmpSuite#{uid}.Suite
        test "sub test", do: assert(true)
      end
      """)
    end

    {suite_dir, uid}
  end

  defp make_single_deployment do
    %Toast.Deployment{
      id: "test-single",
      servers: %{
        "single" => %Toast.Deployment.ServerInfo{
          id: "single",
          role: :single,
          port: 8529,
          endpoint: "http://localhost:8529"
        }
      }
    }
  end

  defp make_cluster_deployment(opts \\ []) do
    coordinators = Keyword.get(opts, :coordinators, 1)
    dbservers = Keyword.get(opts, :dbservers, 3)
    agents = Keyword.get(opts, :agents, 3)

    servers =
      build_servers(:coordinator, coordinators) ++
        build_servers(:dbserver, dbservers) ++
        build_servers(:agent, agents)

    %Toast.Deployment{
      id: "test-cluster",
      servers: Map.new(servers, fn srv -> {srv.id, srv} end)
    }
  end

  defp build_servers(role, count) do
    for i <- 0..(count - 1) do
      id = "#{role}-#{i}"

      %Toast.Deployment.ServerInfo{
        id: id,
        role: role,
        port: 8529 + i,
        endpoint: "http://localhost:#{8529 + i}"
      }
    end
  end
end
