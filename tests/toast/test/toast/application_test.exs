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

defmodule Toast.ApplicationTest do
  use ExUnit.Case, async: true

  describe "supervision tree" do
    test "application starts successfully" do
      # The application is already started by mix test. Verify the
      # top-level supervisor is alive.
      assert Process.whereis(Toast.Supervisor) != nil
      assert Process.alive?(Process.whereis(Toast.Supervisor))
    end

    test "Toast.PortAllocator is running" do
      assert Process.whereis(Toast.PortAllocator) != nil
      assert Process.alive?(Process.whereis(Toast.PortAllocator))
    end

    test "Toast.Process.Supervisor is running" do
      assert Process.whereis(Toast.Process.Supervisor) != nil
      assert Process.alive?(Process.whereis(Toast.Process.Supervisor))
    end

    test "Toast.Deployment.Supervisor is running" do
      assert Process.whereis(Toast.Deployment.Supervisor) != nil
      assert Process.alive?(Process.whereis(Toast.Deployment.Supervisor))
    end

    test "PortAllocator can allocate ports" do
      assert {:ok, port} = Toast.PortAllocator.allocate()
      assert is_integer(port)
      assert port > 0
    end

    test "Process.Supervisor accepts children" do
      # Verify the DynamicSupervisor is functional by checking its child count
      children = DynamicSupervisor.which_children(Toast.Process.Supervisor)
      assert is_list(children)
    end

    test "Deployment.Supervisor accepts children" do
      children = DynamicSupervisor.which_children(Toast.Deployment.Supervisor)
      assert is_list(children)
    end
  end
end
