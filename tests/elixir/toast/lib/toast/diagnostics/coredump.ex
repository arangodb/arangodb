defmodule Toast.Diagnostics.Coredump do
  @moduledoc "Discover and analyze core dump files from crashed ArangoDB server processes."

  alias Toast.Diagnostics.Coredump.{GDB, LLDB, Report}
  require Logger

  @default_timeout_ms 30_000

  @doc """
  Discover core files for a server process.

  Accepts either `:os_pid` (single PID) or `:os_pids` (list of PIDs) to
  search for core dumps from all PIDs that server ever had.
  """
  @spec discover(keyword()) :: [Path.t()]
  def discover(opts) do
    server_dir = Keyword.fetch!(opts, :server_dir)
    os_pids = resolve_os_pids(opts)

    override_dir = System.get_env("TOAST_COREDUMP_DIR")

    paths =
      if override_dir do
        cores_in_dir(override_dir)
      else
        pid_paths =
          Enum.flat_map(os_pids, fn pid ->
            cores_in_tmp(pid) ++ cores_from_pattern(pid)
          end)

        cores_in_dir(server_dir) ++ pid_paths
      end

    paths
    |> Enum.uniq()
    |> Enum.filter(&File.exists?/1)
  end

  @doc "Analyze a core file and extract stack traces."
  @spec analyze(Path.t(), Path.t(), keyword()) :: {:ok, Report.t()} | {:error, term()}
  def analyze(core_path, binary_path, opts \\ []) do
    debugger =
      Keyword.get_lazy(opts, :debugger, fn ->
        case detect_debugger() do
          {:ok, mod} -> mod
          :none -> nil
        end
      end)

    timeout = Keyword.get(opts, :timeout, @default_timeout_ms)

    if debugger == nil do
      {:error, :no_debugger}
    else
      run_debugger(debugger, core_path, binary_path, timeout)
    end
  end

  @doc "Detect available debugger. Prefer LLDB, fall back to GDB."
  @spec detect_debugger() :: {:ok, module()} | :none
  def detect_debugger do
    cond do
      System.find_executable("lldb") -> {:ok, LLDB}
      System.find_executable("gdb") -> {:ok, GDB}
      true -> :none
    end
  end

  @doc "Discover and analyze all core files for a set of servers."
  @spec collect(keyword()) :: [Report.t()]
  def collect(opts) do
    servers = Keyword.get(opts, :servers, [])
    timeout = Keyword.get(opts, :timeout, @default_timeout_ms)
    debugger = Keyword.get(opts, :debugger)

    deadline = System.monotonic_time(:millisecond) + timeout

    Enum.reduce_while(servers, [], fn server, acc ->
      remaining = deadline - System.monotonic_time(:millisecond)

      if remaining <= 0 do
        Logger.warning("Coredump analysis timeout budget exhausted, skipping remaining servers")
        {:halt, acc}
      else
        {:cont, acc ++ collect_for_server(server, remaining, debugger)}
      end
    end)
  end

  # --- Private ---

  defp resolve_os_pids(opts) do
    case Keyword.get(opts, :os_pids) do
      pids when is_list(pids) and pids != [] ->
        pids

      _ ->
        case Keyword.get(opts, :os_pid) do
          nil -> []
          pid -> [pid]
        end
    end
  end

  defp collect_for_server(server, remaining_ms, debugger) do
    discover_opts =
      case Map.get(server, :os_pids) do
        pids when is_list(pids) and pids != [] ->
          [server_dir: server.server_dir, os_pids: pids]

        _ ->
          [server_dir: server.server_dir, os_pid: server.os_pid]
      end

    cores = discover(discover_opts)

    if cores == [] do
      []
    else
      Logger.info("Found #{length(cores)} core file(s) for #{server.id}")
      per_core_timeout = max(1_000, div(remaining_ms, length(cores)))

      analyze_opts =
        [timeout: per_core_timeout] ++ if(debugger, do: [debugger: debugger], else: [])

      Enum.flat_map(cores, &analyze_core(&1, server.binary_path, analyze_opts))
    end
  end

  defp analyze_core(core_path, binary_path, opts) do
    case analyze(core_path, binary_path, opts) do
      {:ok, report} ->
        [report]

      {:error, reason} ->
        Logger.warning("Failed to analyze core #{core_path}: #{inspect(reason)}")
        []
    end
  end

  defp run_debugger(debugger, core_path, binary_path, timeout) do
    executable = debugger.executable()
    args = debugger.command(binary_path, core_path)

    case cmd_with_timeout(executable, args, timeout) do
      {:ok, output, 0} ->
        build_report(debugger, core_path, binary_path, output)

      {:ok, output, exit_code} ->
        result = debugger.parse_output(output)

        if result.threads != [] do
          build_report_from_parsed(debugger, core_path, binary_path, result)
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

  defp build_report(debugger, core_path, binary_path, output) do
    result = debugger.parse_output(output)
    build_report_from_parsed(debugger, core_path, binary_path, result)
  end

  defp build_report_from_parsed(debugger, core_path, binary_path, result) do
    {:ok,
     %Report{
       core_path: core_path,
       binary_path: binary_path,
       debugger: debugger_name(debugger),
       signal: result.signal,
       faulting_address: result.faulting_address,
       threads: result.threads,
       crash_thread: result.crash_thread
     }}
  end

  defp cmd_with_timeout(executable, args, timeout) do
    exec_path = System.find_executable(executable)

    if exec_path == nil do
      {:error, :executable_not_found}
    else
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
    e -> {:error, e}
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

  defp cores_in_dir(nil), do: []

  defp cores_in_dir(dir) do
    if File.dir?(dir) do
      Path.wildcard(Path.join(dir, "core*"))
    else
      []
    end
  end

  defp cores_in_tmp(nil), do: []

  defp cores_in_tmp(os_pid) do
    pid_str = to_string(os_pid)

    "/tmp/core*"
    |> Path.wildcard()
    |> Enum.filter(fn path ->
      # Match PID as a whole segment delimited by dots/dashes/underscores
      # to avoid false positives (e.g., PID "12" matching "core.1234")
      basename = Path.basename(path)
      segments = String.split(basename, ~r/[.\-_]/)
      pid_str in segments
    end)
  end

  defp cores_from_pattern(os_pid) do
    case File.read("/proc/sys/kernel/core_pattern") do
      {:ok, pattern} ->
        pattern = String.trim(pattern)

        if String.starts_with?(pattern, "|") do
          cores_from_coredumpctl(os_pid)
        else
          cores_from_fs_pattern(pattern, os_pid)
        end

      {:error, _} ->
        []
    end
  end

  defp cores_from_coredumpctl(nil), do: []

  defp cores_from_coredumpctl(os_pid) do
    if System.find_executable("coredumpctl") do
      extract_coredumpctl_path(os_pid)
    else
      []
    end
  rescue
    _ -> []
  end

  defp extract_coredumpctl_path(os_pid) do
    case cmd_with_timeout("coredumpctl", ["info", to_string(os_pid)], 5_000) do
      {:ok, output, 0} ->
        case Regex.run(~r/Storage:\s+(\S+)/, output) do
          [_, path] -> [String.trim(path)]
          _ -> []
        end

      _ ->
        []
    end
  end

  defp cores_from_fs_pattern(pattern, os_pid) do
    expanded =
      pattern
      |> String.replace("%%", "\x00")
      |> String.replace("%p", to_string(os_pid || "*"))
      |> String.replace(~r/%[eEhtugs]/, "*")
      |> String.replace("%c", "*")
      |> String.replace("\x00", "%")

    if String.contains?(expanded, "/") do
      Path.wildcard(expanded)
    else
      []
    end
  end
end
