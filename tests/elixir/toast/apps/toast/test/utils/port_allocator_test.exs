defmodule Toast.PortAllocatorTest do
  use ExUnit.Case, async: false

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

  describe "allocate!/1" do
    test "returns the port integer directly", context do
      name = unique_name(context)
      pid = start_supervised!({PortAllocator, name: name, base_port: 40_500})

      port = PortAllocator.allocate!(pid)
      assert is_integer(port)
      assert port > 0
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
end
