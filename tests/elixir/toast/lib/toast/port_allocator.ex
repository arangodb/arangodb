defmodule Toast.PortAllocator do
  @moduledoc """
  Allocates available network ports by probing with a socket bind.

  Supports two strategies (configurable via `:strategy` option):
    * `:sequential` (default) — increments from a base port, skipping occupied ones.
    * `:random` — picks random ports from the configured range, verifying availability.
  """

  use GenServer

  @min_port 8529
  @max_port 59_999
  @max_attempts 1000

  # Client API

  @doc """
  Start the port allocator.

  ## Options
    * `:strategy` - `:sequential` (default) or `:random`
    * `:base_port` - starting port for sequential strategy (default: random in #{@min_port}..#{@max_port})
    * `:min_port` - minimum port for random strategy (default: #{@min_port})
    * `:max_port` - maximum port for random strategy (default: #{@max_port})
    * `:name` - GenServer name (default: `__MODULE__`)
  """
  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts \\ []) do
    name = Keyword.get(opts, :name, __MODULE__)
    strategy = Keyword.get(opts, :strategy, :sequential)
    GenServer.start_link(__MODULE__, {strategy, opts}, name: name)
  end

  @doc """
  Allocate an available port.
  Returns `{:ok, port}` or `{:error, :no_ports_available}`.
  """
  @spec allocate(GenServer.server()) :: {:ok, pos_integer()} | {:error, :no_ports_available}
  def allocate(server \\ __MODULE__) do
    GenServer.call(server, :allocate)
  end

  @doc """
  Allocate `count` ports atomically in a single GenServer call.
  """
  @spec allocate_batch(GenServer.server(), pos_integer()) ::
          {:ok, [pos_integer()]} | {:error, :no_ports_available}
  def allocate_batch(server \\ __MODULE__, count) do
    GenServer.call(server, {:allocate_batch, count})
  end

  # Server callbacks

  @impl true
  def init({:sequential, opts}) do
    base_port = Keyword.get(opts, :base_port, random_port())
    {:ok, %{strategy: :sequential, next_port: base_port}}
  end

  def init({:random, opts}) do
    min = Keyword.get(opts, :min_port, @min_port)
    max = Keyword.get(opts, :max_port, @max_port)
    {:ok, %{strategy: :random, min_port: min, max_port: max, allocated: MapSet.new()}}
  end

  @impl true
  def handle_call(:allocate, _from, %{strategy: :sequential} = state) do
    case find_sequential(state.next_port, 0) do
      {:ok, port} ->
        {:reply, {:ok, port}, %{state | next_port: port + 1}}

      :error ->
        {:reply, {:error, :no_ports_available}, state}
    end
  end

  def handle_call(:allocate, _from, %{strategy: :random} = state) do
    case find_random(state.min_port, state.max_port, state.allocated, 0) do
      {:ok, port} ->
        {:reply, {:ok, port}, %{state | allocated: MapSet.put(state.allocated, port)}}

      :error ->
        {:reply, {:error, :no_ports_available}, state}
    end
  end

  @impl true
  def handle_call({:allocate_batch, count}, _from, %{strategy: :sequential} = state) do
    case allocate_sequential_batch(state.next_port, count, []) do
      {:ok, ports, next_port} ->
        {:reply, {:ok, ports}, %{state | next_port: next_port}}

      :error ->
        {:reply, {:error, :no_ports_available}, state}
    end
  end

  def handle_call({:allocate_batch, count}, _from, %{strategy: :random} = state) do
    case allocate_random_batch(state.min_port, state.max_port, state.allocated, count, []) do
      {:ok, ports, allocated} ->
        {:reply, {:ok, ports}, %{state | allocated: allocated}}

      :error ->
        {:reply, {:error, :no_ports_available}, state}
    end
  end

  # Sequential strategy

  defp allocate_sequential_batch(next_port, 0, acc), do: {:ok, Enum.reverse(acc), next_port}

  defp allocate_sequential_batch(port, remaining, acc) do
    case find_sequential(port, 0) do
      {:ok, found} -> allocate_sequential_batch(found + 1, remaining - 1, [found | acc])
      :error -> :error
    end
  end

  defp find_sequential(_port, attempts) when attempts >= @max_attempts, do: :error

  defp find_sequential(port, attempts) do
    if port_available?(port) do
      {:ok, port}
    else
      find_sequential(port + 1, attempts + 1)
    end
  end

  # Random strategy

  defp allocate_random_batch(_min, _max, allocated, 0, acc), do: {:ok, Enum.reverse(acc), allocated}

  defp allocate_random_batch(min, max, allocated, remaining, acc) do
    case find_random(min, max, allocated, 0) do
      {:ok, port} ->
        allocate_random_batch(min, max, MapSet.put(allocated, port), remaining - 1, [port | acc])

      :error ->
        :error
    end
  end

  defp find_random(_min, _max, _allocated, attempts) when attempts >= @max_attempts, do: :error

  defp find_random(min, max, allocated, attempts) do
    port = Enum.random(min..max)

    if port not in allocated and port_available?(port) do
      {:ok, port}
    else
      find_random(min, max, allocated, attempts + 1)
    end
  end

  # Shared

  defp port_available?(port) do
    case :gen_tcp.listen(port, [:binary, {:reuseaddr, true}]) do
      {:ok, socket} ->
        :gen_tcp.close(socket)
        true

      {:error, _} ->
        false
    end
  end

  defp random_port do
    Enum.random(@min_port..@max_port)
  end
end
