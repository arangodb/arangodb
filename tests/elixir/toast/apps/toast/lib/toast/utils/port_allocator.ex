defmodule Toast.PortAllocator do
  @moduledoc """
  Allocates available network ports by probing with a socket bind.

  Uses an incrementing counter starting from a configurable base port.
  Each candidate port is tested by attempting to bind a TCP socket —
  if binding succeeds, the port is available and returned.
  """

  use GenServer

  @min_port 8529
  @max_port 59_999
  @max_attempts 1000

  # Client API

  @doc """
  Start the port allocator.

  ## Options
    * `:base_port` - starting port number (default: random in #{@min_port}..#{@max_port})
    * `:name` - GenServer name (default: `__MODULE__`)
  """
  @spec start_link(keyword()) :: GenServer.on_start()
  def start_link(opts \\ []) do
    name = Keyword.get(opts, :name, __MODULE__)
    base_port = Keyword.get(opts, :base_port, random_base_port())
    GenServer.start_link(__MODULE__, base_port, name: name)
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
  Allocate an available port, raising on failure.
  """
  @spec allocate!(GenServer.server()) :: pos_integer()
  def allocate!(server \\ __MODULE__) do
    case allocate(server) do
      {:ok, port} -> port
      {:error, reason} -> raise "Failed to allocate port: #{inspect(reason)}"
    end
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
  def init(base_port) do
    {:ok, %{next_port: base_port}}
  end

  @impl true
  def handle_call(:allocate, _from, state) do
    case find_available_port(state.next_port, 0) do
      {:ok, port} ->
        {:reply, {:ok, port}, %{state | next_port: port + 1}}

      :error ->
        {:reply, {:error, :no_ports_available}, state}
    end
  end

  @impl true
  def handle_call({:allocate_batch, count}, _from, state) do
    case allocate_ports(state.next_port, count, []) do
      {:ok, ports, next_port} ->
        {:reply, {:ok, ports}, %{state | next_port: next_port}}

      :error ->
        {:reply, {:error, :no_ports_available}, state}
    end
  end

  defp allocate_ports(next_port, 0, acc), do: {:ok, Enum.reverse(acc), next_port}

  defp allocate_ports(port, remaining, acc) do
    case find_available_port(port, 0) do
      {:ok, found} -> allocate_ports(found + 1, remaining - 1, [found | acc])
      :error -> :error
    end
  end

  defp find_available_port(_port, attempts) when attempts >= @max_attempts, do: :error

  defp find_available_port(port, attempts) do
    if port_available?(port) do
      {:ok, port}
    else
      find_available_port(port + 1, attempts + 1)
    end
  end

  defp port_available?(port) do
    case :gen_tcp.listen(port, [:binary, {:reuseaddr, true}]) do
      {:ok, socket} ->
        :gen_tcp.close(socket)
        true

      {:error, _} ->
        false
    end
  end

  defp random_base_port do
    Enum.random(@min_port..@max_port)
  end
end
