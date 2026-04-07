defmodule Toast.Diagnostics.Coredump do
  @moduledoc "Discover and analyze core dump files from crashed ArangoDB server processes."

  alias Toast.Diagnostics.Coredump.{GDB, LLDB, Report}
  require Logger

  @default_timeout_ms 180_000

  @doc """
  Discover core files for a server process.

  Accepts either `:os_pid` (single PID) or `:os_pids` (list of PIDs) to
  search for core dumps from all PIDs that server ever had.
  """
  @spec discover(keyword()) :: [Path.t()]
  def discover(opts) do
    server_dir = Keyword.fetch!(opts, :server_dir)
    os_pids = resolve_os_pids(opts)

    override_dir = Keyword.get(opts, :coredump_dir)

    paths =
      case override_dir do
        nil ->
          Logger.debug("Coredump discover: server_dir=#{server_dir} pids=#{inspect(os_pids)}")
          core_pattern = read_core_pattern()

          pid_paths =
            Enum.flat_map(os_pids, &(cores_in_tmp(&1) ++ cores_from_pattern(&1, core_pattern)))

          cores_in_dir(server_dir) ++ pid_paths

        dir ->
          Logger.debug("Coredump discover: using override dir #{dir}")
          cores_in_override_dir(dir, os_pids)
      end

    not_before = to_posix(Keyword.get(opts, :not_before))

    result =
      paths
      |> Enum.uniq()
      |> Enum.filter(&valid_core_file?(&1, not_before))

    Logger.debug("Coredump discover: found #{length(result)} core file(s)")
    result
  end

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

  defp resolve_debugger_from_opts(opts) do
    case Keyword.fetch(opts, :debugger) do
      {:ok, {mod, exec}} -> {mod, exec}
      {:ok, mod} when is_atom(mod) and not is_nil(mod) -> {mod, mod.executable()}
      {:ok, nil} -> {nil, nil}
      :error -> resolve_debugger(:auto) || {nil, nil}
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

  defp resolve_os_pids(opts) do
    case Keyword.get(opts, :os_pids) do
      [_ | _] = pids ->
        pids

      _ ->
        case Keyword.get(opts, :os_pid) do
          nil -> []
          pid -> [pid]
        end
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

  defp cmd_with_timeout(executable, args, timeout) do
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

  @doc """
  Returns a warning string if the current system's coredump handler is unknown
  and no override directory is configured. Returns nil otherwise.
  """
  @spec coredump_discovery_warning(Path.t() | nil) :: String.t() | nil
  def coredump_discovery_warning(coredump_dir)
  def coredump_discovery_warning(dir) when not is_nil(dir), do: nil

  def coredump_discovery_warning(nil) do
    case File.read("/proc/sys/kernel/core_pattern") do
      {:ok, raw} ->
        pattern = String.trim(raw)
        handler = pipe_handler_name(pattern)

        if String.starts_with?(pattern, "|") and
             not (handler =~ "apport") and
             System.find_executable("coredumpctl") == nil do
          "Coredump discovery may not work: core_pattern pipes to unknown handler '#{handler}'. " <>
            "Set TOAST_COREDUMP_DIR or --coredump-dir to the directory where your system stores core dumps."
        end

      _ ->
        nil
    end
  end

  defp to_posix(nil), do: nil
  defp to_posix(%DateTime{} = dt), do: DateTime.to_unix(dt)
  defp to_posix(ts) when is_integer(ts), do: ts

  defp valid_core_file?(path, not_before) do
    case File.stat(path, time: :posix) do
      {:ok, %{mtime: mtime}} ->
        is_nil(not_before) or mtime >= not_before

      {:error, _} ->
        false
    end
  end

  # The server work directory is exclusive to this server instance, so any
  # core* file in it is ours — no PID filtering needed.
  defp cores_in_dir(nil), do: []

  defp cores_in_dir(dir) do
    if File.dir?(dir) do
      Path.wildcard(Path.join(dir, "core*"))
    else
      []
    end
  end

  # The override directory is shared and may contain cores from unrelated
  # processes, so we filter by PID in the filename.
  defp cores_in_override_dir(dir, os_pids) do
    if File.dir?(dir) do
      all_files = Path.wildcard(Path.join(dir, "*"))
      result = filter_cores_by_pid(all_files, os_pids)

      Logger.debug(
        "Coredump: override dir has #{length(all_files)} file(s), #{length(result)} matched"
      )

      result
    else
      Logger.warning("Coredump override directory does not exist: #{dir}")
      []
    end
  end

  defp filter_cores_by_pid(files, []), do: files

  defp filter_cores_by_pid(files, os_pids) do
    pid_strings = Enum.map(os_pids, &to_string/1)

    Enum.filter(files, fn path ->
      Enum.any?(pid_strings, &filename_contains_pid?(path, &1))
    end)
  end

  defp cores_in_tmp(nil), do: []

  defp cores_in_tmp(os_pid) do
    pid_str = to_string(os_pid)

    "/tmp/core*"
    |> Path.wildcard()
    |> Enum.filter(&filename_contains_pid?(&1, pid_str))
  end

  defp read_core_pattern do
    case File.read("/proc/sys/kernel/core_pattern") do
      {:ok, pattern} -> {:ok, String.trim(pattern)}
      {:error, _} -> :error
    end
  end

  defp cores_from_pattern(_os_pid, :error), do: []

  defp cores_from_pattern(os_pid, {:ok, pattern}) do
    if String.starts_with?(pattern, "|") do
      cores_from_piped_handler(pattern, os_pid)
    else
      cores_from_fs_pattern(pattern, os_pid)
    end
  end

  defp pipe_handler_name(pattern) do
    pattern |> String.trim_leading("|") |> String.split() |> hd() |> Path.basename()
  end

  defp filename_contains_pid?(path, pid_str) do
    path |> Path.basename() |> String.split(~r/[.\-_]/) |> Enum.member?(pid_str)
  end

  defp cores_from_piped_handler(pattern, os_pid) do
    handler = pipe_handler_name(pattern)
    Logger.debug("Coredump: detected piped handler '#{handler}'")

    cond do
      handler =~ "apport" -> cores_from_apport(os_pid)
      System.find_executable("coredumpctl") -> extract_coredumpctl_path(os_pid)
      true -> []
    end
  end

  defp cores_from_apport(nil), do: []

  defp cores_from_apport(os_pid) do
    crash_dir = "/var/crash"
    Logger.debug("Coredump: searching apport crash directory #{crash_dir}")

    if File.dir?(crash_dir) do
      pid_str = to_string(os_pid)

      crash_dir
      |> Path.join("*.#{pid_str}.crash")
      |> Path.wildcard()
      |> Enum.flat_map(&extract_apport_core/1)
    else
      []
    end
  end

  defp extract_apport_core(crash_file) do
    dest_dir = crash_file <> ".unpacked"
    File.mkdir_p(dest_dir)

    case cmd_with_timeout("apport-unpack", [crash_file, dest_dir], 10_000) do
      {:ok, _, 0} ->
        core_path = Path.join(dest_dir, "CoreDump")
        if File.exists?(core_path), do: [core_path], else: []

      _ ->
        Logger.warning("Failed to unpack apport crash report: #{crash_file}")
        []
    end
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

    Logger.debug("Coredump: searching fs pattern #{expanded}")

    if String.contains?(expanded, "/") do
      Path.wildcard(expanded)
    else
      []
    end
  end
end
