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

defmodule ToastTest.StateCleanupTest do
  use ExUnit.Case, async: false

  import Toast.DeploymentTestHelpers, only: [setup_deployment_registry: 1]

  alias ToastTest.{Abort, DeploymentRegistry, StateCleanup}

  setup :setup_deployment_registry

  setup do
    original_after_suite = Application.get_env(:ex_unit, :after_suite, [])

    on_exit(fn ->
      Application.put_env(:ex_unit, :after_suite, original_after_suite)
    end)

    :ok
  end

  test "reset clears deployment registry" do
    DeploymentRegistry.put(TestSuite, %{id: "dep-1"})
    assert DeploymentRegistry.get(TestSuite) == %{id: "dep-1"}

    StateCleanup.reset()

    assert_raise RuntimeError, fn -> DeploymentRegistry.get(TestSuite) end
  end

  test "reset clears abort state" do
    Abort.clear!()
    Abort.abort!("test reason")
    assert Abort.reason() == "test reason"

    StateCleanup.reset()

    assert Abort.reason() == nil
  end

  test "reset clears abort state when already clear" do
    Abort.clear!()
    assert Abort.reason() == nil

    StateCleanup.reset()

    assert Abort.reason() == nil
  end

  test "reset clears after_suite callbacks" do
    Application.put_env(:ex_unit, :after_suite, [fn _ -> :ok end])

    StateCleanup.reset()

    assert Application.get_env(:ex_unit, :after_suite) == []
  end

  test "reset does not touch port allocator state" do
    try do
      :ets.delete(:toast_port_allocator)
    catch
      :error, :badarg -> :ok
    end

    :ets.new(:toast_port_allocator, [:named_table, :set, :public])
    :ets.insert(:toast_port_allocator, {:next_port, 8530})

    StateCleanup.reset()

    assert :ets.lookup(:toast_port_allocator, :next_port) == [{:next_port, 8530}]
  after
    try do
      :ets.delete(:toast_port_allocator)
    catch
      :error, :badarg -> :ok
    end
  end
end
