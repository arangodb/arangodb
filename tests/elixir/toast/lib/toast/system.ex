defmodule Toast.System do
  @moduledoc """
  System resource detection.

  Detects available memory, preferring cgroup v2 limits (containers)
  over `/proc/meminfo` (bare metal).
  """

  @cgroup_v2_memory_max "/sys/fs/cgroup/memory.max"
  @proc_meminfo "/proc/meminfo"

  @doc """
  Detect the total available memory in bytes.

  Checks cgroup v2 first (for containerized CI), then falls back to
  `/proc/meminfo`. Returns `{:ok, bytes}` or `:error`.
  """
  @spec total_memory() :: {:ok, pos_integer()} | :error
  def total_memory do
    with :error <- read_cgroup_v2_memory() do
      read_proc_meminfo()
    end
  end

  defp read_cgroup_v2_memory do
    case File.read(@cgroup_v2_memory_max) do
      {:ok, content} ->
        case parse_cgroup_memory_max(content) do
          {:ok, _bytes} = ok -> ok
          # "max" means unlimited — fall through to /proc/meminfo
          _ -> :error
        end

      {:error, _} ->
        :error
    end
  end

  defp read_proc_meminfo do
    case File.read(@proc_meminfo) do
      {:ok, content} -> parse_meminfo(content)
      {:error, _} -> :error
    end
  end

  @doc """
  Parse the content of `/proc/meminfo` and extract MemTotal in bytes.
  """
  @spec parse_meminfo(String.t()) :: {:ok, pos_integer()} | :error
  def parse_meminfo(content) do
    case Regex.run(~r/MemTotal:\s+(\d+)\s+kB/, content) do
      [_, kb_str] -> {:ok, String.to_integer(kb_str) * 1024}
      nil -> :error
    end
  end

  @doc """
  Parse the content of `/sys/fs/cgroup/memory.max`.

  Returns `{:ok, bytes}` for a numeric limit, `:unlimited` for "max",
  or `:error` if unparseable.
  """
  @spec parse_cgroup_memory_max(String.t()) :: {:ok, pos_integer()} | :unlimited | :error
  def parse_cgroup_memory_max(content) do
    trimmed = String.trim(content)

    cond do
      trimmed == "" -> :error
      trimmed == "max" -> :unlimited
      true -> parse_integer(trimmed)
    end
  end

  defp parse_integer(str) do
    case Integer.parse(str) do
      {n, ""} when n > 0 -> {:ok, n}
      _ -> :error
    end
  end
end
