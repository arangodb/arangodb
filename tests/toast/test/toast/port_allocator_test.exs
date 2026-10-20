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

defmodule Toast.PortAllocatorTest do
  use ExUnit.Case, async: true

  alias Toast.PortAllocator

  defp unique_name(context) do
    :"port_allocator_#{context.test}"
  end

  describe "allocate/1" do
    test "returns {:ok, port} with a positive integer", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 40_000})

      assert {:ok, port} = PortAllocator.allocate(pid)
      assert is_integer(port)
      assert port > 0
    end

    test "allocated port is actually available for binding", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 40_100})

      {:ok, port} = PortAllocator.allocate(pid)
      assert {:ok, socket} = :gen_tcp.listen(port, [:binary, reuseaddr: true])
      :gen_tcp.close(socket)
    end

    test "sequential allocations return incrementing ports", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 40_200})

      {:ok, port1} = PortAllocator.allocate(pid)
      {:ok, port2} = PortAllocator.allocate(pid)
      {:ok, port3} = PortAllocator.allocate(pid)

      assert port2 > port1
      assert port3 > port2
    end

    test "skips occupied ports", context do
      name = unique_name(context)
      base_port = 40_300

      {:ok, blocker} = :gen_tcp.listen(base_port, [:binary, reuseaddr: true])

      on_exit(fn -> :gen_tcp.close(blocker) end)

      pid = start_supervised!({PortAllocator, name: name, base_port: base_port})

      {:ok, port} = PortAllocator.allocate(pid)
      assert port > base_port
    end

    test "multiple allocations never return duplicates", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 40_400})

      ports =
        for _ <- 1..10 do
          {:ok, port} = PortAllocator.allocate(pid)
          port
        end

      assert length(Enum.uniq(ports)) == 10
    end
  end

  describe "start_link/1" do
    test "respects custom base_port", context do
      name = unique_name(context)
      base_port = 41_000
      pid = start_supervised!({PortAllocator, name: name, base_port: base_port})

      {:ok, port} = PortAllocator.allocate(pid)
      assert port >= base_port
    end

    test "works with a custom name", context do
      name = unique_name(context)
      start_supervised!({PortAllocator, name: name, base_port: 41_100})

      assert {:ok, port} = PortAllocator.allocate(name)
      assert is_integer(port)
      assert port >= 41_100
    end
  end

  describe "allocate_batch/2" do
    test "returns correct count", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 42_000})

      assert {:ok, ports} = PortAllocator.allocate_batch(pid, 5)
      assert length(ports) == 5
    end

    test "all ports are unique", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 42_100})

      {:ok, ports} = PortAllocator.allocate_batch(pid, 5)

      assert MapSet.size(MapSet.new(ports)) == length(ports)
    end

    test "ports are >= base_port", context do
      name = unique_name(context)
      base_port = 42_200
      pid = start_supervised!({PortAllocator, name: name, base_port: base_port})

      {:ok, ports} = PortAllocator.allocate_batch(pid, 5)

      Enum.each(ports, fn port ->
        assert port >= base_port
      end)
    end

    test "single port batch", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 42_300})

      assert {:ok, [port]} = PortAllocator.allocate_batch(pid, 1)
      assert is_integer(port)
      assert port >= 42_300
    end

    test "subsequent allocations don't overlap", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 42_400})

      {:ok, batch_ports} = PortAllocator.allocate_batch(pid, 3)
      {:ok, next_port} = PortAllocator.allocate(pid)

      refute next_port in batch_ports
    end
  end

  describe "random strategy" do
    test "returns {:ok, port} with a positive integer", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, strategy: :random})

      assert {:ok, port} = PortAllocator.allocate(pid)
      assert is_integer(port)
      assert port > 0
    end

    test "allocated port is actually available for binding", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, strategy: :random})

      {:ok, port} = PortAllocator.allocate(pid)
      assert {:ok, socket} = :gen_tcp.listen(port, [:binary, reuseaddr: true])
      :gen_tcp.close(socket)
    end

    test "ports within configured range", context do
      name = unique_name(context)
      min = 45_000
      max = 45_100

      pid =
        start_supervised!(
          {PortAllocator, name: name, strategy: :random, min_port: min, max_port: max}
        )

      for _ <- 1..10 do
        {:ok, port} = PortAllocator.allocate(pid)
        assert port >= min
        assert port <= max
      end
    end

    test "multiple allocations never return duplicates", context do
      name = unique_name(context)

      pid =
        start_supervised!(
          {PortAllocator, name: name, strategy: :random, min_port: 46_000, max_port: 46_100}
        )

      ports =
        for _ <- 1..10 do
          {:ok, port} = PortAllocator.allocate(pid)
          port
        end

      assert length(Enum.uniq(ports)) == 10
    end

    test "skips occupied ports", context do
      name = unique_name(context)
      # Use a very narrow range so the occupied port must be skipped
      min = 47_000
      max = 47_001

      {:ok, blocker} = :gen_tcp.listen(min, [:binary, reuseaddr: true])
      on_exit(fn -> :gen_tcp.close(blocker) end)

      pid =
        start_supervised!(
          {PortAllocator, name: name, strategy: :random, min_port: min, max_port: max}
        )

      {:ok, port} = PortAllocator.allocate(pid)
      assert port == max
    end

    test "batch returns correct count", context do
      name = unique_name(context)

      pid =
        start_supervised!(
          {PortAllocator, name: name, strategy: :random, min_port: 48_000, max_port: 48_100}
        )

      assert {:ok, ports} = PortAllocator.allocate_batch(pid, 5)
      assert length(ports) == 5
      assert MapSet.size(MapSet.new(ports)) == 5
    end

    test "batch ports don't overlap with subsequent allocations", context do
      name = unique_name(context)

      pid =
        start_supervised!(
          {PortAllocator, name: name, strategy: :random, min_port: 49_000, max_port: 49_100}
        )

      {:ok, batch_ports} = PortAllocator.allocate_batch(pid, 3)
      {:ok, next_port} = PortAllocator.allocate(pid)

      refute next_port in batch_ports
    end
  end
end
