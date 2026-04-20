defmodule Toast.Diagnostics.Coredump do
  @moduledoc "Discover and analyze core dump files from crashed ArangoDB server processes."

  alias Toast.Diagnostics.Coredump.{GDB, LLDB, Report}
  require Logger

  @default_timeout_ms 180_000

  @doc "Analyze a core file and extract stack traces."
  @spec analyze(Path.t(), Path.t(), keyword()) :: {:ok, Report.t()} | {:error, term()}
  def analyze(core_path, binary_path, opts \\ []) do
    {debugger_mod, executable} = resolve_debugger_from_opts(opts)

    timeout = Keyword.get(opts, :timeout, @default_timeout_ms)

    Logger.info(
      "Analyzing core #{Path.basename(core_path)} with #{executable || "no debugger"} (timeout=#{timeout}ms)"
    )

    start = System.monotonic_time(:millisecond)

    result =
      case debugger_mod do
        nil -> {:error, :no_debugger}
        mod -> run_debugger(mod, executable, core_path, binary_path, timeout)
      end

    elapsed = System.monotonic_time(:millisecond) - start

    case result do
      {:ok, report} ->
        Logger.info(
          "Core analysis succeeded for #{Path.basename(core_path)} " <>
            "(#{length(report.threads)} threads, #{elapsed}ms)"
        )

      {:error, reason} ->
        Logger.warning(
          "Core analysis failed for #{Path.basename(core_path)}: #{inspect(reason)} (#{elapsed}ms)"
        )
    end

    result
  end

  # --- Debugger resolution ---

  @doc """
  Detect available debugger. Prefer LLDB, fall back to GDB.
  Returns `{:ok, module, executable_path}` or `:none`.
  Also finds versioned variants like `lldb-18` or `gdb-14`.
  """
  @spec detect_debugger() :: {:ok, module(), String.t()} | :none
  def detect_debugger do
    case find_executable_variant("lldb") do
      {:ok, path} ->
        {:ok, LLDB, path}

      :not_found ->
        case find_executable_variant("gdb") do
          {:ok, path} -> {:ok, GDB, path}
          :not_found -> :none
        end
    end
  end

  @doc """
  Resolve a debugger from a config atom (`:lldb`, `:gdb`, `:auto`, `:none`).
  Returns `{module, executable_path}` or `nil`.
  """
  @spec resolve_debugger(atom()) :: {module(), String.t()} | nil
  def resolve_debugger(:none), do: nil
  def resolve_debugger(nil), do: nil

  def resolve_debugger(:auto) do
    case detect_debugger() do
      {:ok, mod, path} -> {mod, path}
      :none -> nil
    end
  end

  def resolve_debugger(name) when name in [:lldb, :gdb] do
    {mod, label} = debugger_for_name(name)

    case find_executable_variant(label) do
      {:ok, path} ->
        {mod, path}

      :not_found ->
        Logger.warning("Debugger :#{name} configured but not found in PATH")
        nil
    end
  end

  # --- Subprocess execution (shared with Discovery) ---

  @doc false
  @spec cmd_with_timeout(String.t(), [String.t()], non_neg_integer()) ::
          {:ok, String.t(), non_neg_integer()} | {:error, term()}
  def cmd_with_timeout(executable, args, timeout) do
    case System.find_executable(executable) do
      nil ->
        {:error, :executable_not_found}

      exec_path ->
        port =
          Port.open(
            {:spawn_executable, exec_path},
            [:binary, :exit_status, :stderr_to_stdout, args: args]
          )

        os_pid =
          case Port.info(port, :os_pid) do
            {:os_pid, pid} -> pid
            nil -> nil
          end

        deadline = System.monotonic_time(:millisecond) + timeout
        collect_port_output(port, os_pid, [], deadline)
    end
  rescue
    e in [ErlangError, SystemLimitError] ->
      Logger.warning("cmd_with_timeout failed: #{inspect(e)}")
      {:error, e}
  end

  # --- Private ---

  defp resolve_debugger_from_opts(opts) do
    case Keyword.fetch(opts, :debugger) do
      {:ok, {mod, exec}} -> {mod, exec}
      {:ok, mod} when is_atom(mod) and not is_nil(mod) -> {mod, mod.executable()}
      {:ok, nil} -> {nil, nil}
      :error -> resolve_debugger(:auto) || {nil, nil}
    end
  end

  defp debugger_for_name(:lldb), do: {LLDB, "lldb"}
  defp debugger_for_name(:gdb), do: {GDB, "gdb"}

  defp find_executable_variant(name) do
    case System.find_executable(name) do
      nil -> find_versioned_executable(name)
      path -> {:ok, path}
    end
  end

  defp find_versioned_executable(name) do
    # Search PATH for versioned variants (e.g., lldb-18, gdb-14)
    path_dirs = System.get_env("PATH", "") |> String.split(":")

    result =
      path_dirs
      |> Stream.flat_map(fn dir ->
        case File.ls(dir) do
          {:ok, entries} -> Enum.filter(entries, &String.starts_with?(&1, name <> "-"))
          _ -> []
        end
      end)
      |> Stream.filter(fn entry ->
        # Must be name-<version> (not e.g. "lldb-server")
        suffix = String.trim_leading(entry, name <> "-")
        Regex.match?(~r/^\d/, suffix)
      end)
      |> Enum.take(1)

    case result do
      [variant] -> {:ok, System.find_executable(variant) || variant}
      [] -> :not_found
    end
  end

  defp run_debugger(debugger, executable, core_path, binary_path, timeout) do
    args = debugger.command(binary_path, core_path)
    Logger.debug("Coredump: running #{executable} #{Enum.join(args, " ")}")

    case cmd_with_timeout(executable, args, timeout) do
      {:ok, output, 0} ->
        result = debugger.parse_output(output)
        build_report(debugger, core_path, binary_path, result)

      {:ok, output, exit_code} ->
        result = debugger.parse_output(output)

        if result.threads != [] do
          build_report(debugger, core_path, binary_path, result)
        else
          {:error, {:debugger_failed, exit_code, output}}
        end

      {:error, :timeout} ->
        Logger.warning("Debugger timed out analyzing #{core_path}")
        {:error, :timeout}

      {:error, reason} ->
        Logger.warning("Debugger error analyzing #{core_path}: #{inspect(reason)}")
        {:error, {:debugger_error, reason}}
    end
  end

  defp build_report(debugger, core_path, binary_path, result) do
    {:ok,
     %Report{
       core_path: core_path,
       binary_path: binary_path,
       debugger: debugger_name(debugger),
       signal: result.signal,
       faulting_address: result.faulting_address,
       registers: result[:registers],
       disassembly: result[:disassembly],
       threads: result.threads,
       crash_thread: result.crash_thread
     }}
  end

  defp collect_port_output(port, os_pid, chunks, deadline) do
    remaining = max(0, deadline - System.monotonic_time(:millisecond))

    receive do
      {^port, {:data, data}} ->
        collect_port_output(port, os_pid, [data | chunks], deadline)

      {^port, {:exit_status, code}} ->
        output = chunks |> Enum.reverse() |> IO.iodata_to_binary()
        {:ok, output, code}
    after
      remaining ->
        kill_os_process(os_pid)
        safe_port_close(port)
        {:error, :timeout}
    end
  end

  defp kill_os_process(nil), do: :ok

  defp kill_os_process(os_pid) do
    System.cmd("kill", ["-9", to_string(os_pid)])
  rescue
    _ -> :ok
  end

  defp safe_port_close(port) do
    Port.close(port)
  catch
    _, _ -> :ok
  end

  defp debugger_name(GDB), do: :gdb
  defp debugger_name(LLDB), do: :lldb
  defp debugger_name(_), do: :unknown
end
