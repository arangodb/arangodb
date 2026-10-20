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

defmodule Toast.Diagnostics.Coredump.Discovery do
  @moduledoc false
  # Core dump discovery across platform-specific locations.
  #
  # Searches: server work directory, /tmp, filesystem core_pattern,
  # apport crash reports, and coredumpctl (systemd).

  require Logger

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

  @spec coredump_discovery_warning(Path.t() | nil) :: String.t() | nil
  def coredump_discovery_warning(dir) when not is_nil(dir), do: nil

  def coredump_discovery_warning(nil) do
    case File.read("/proc/sys/kernel/core_pattern") do
      {:ok, raw} -> diagnose_core_pattern(String.trim(raw))
      _ -> nil
    end
  end

  @doc false
  # Exposed for testing. Analyzes a core_pattern string and returns a warning
  # if the configuration is likely to prevent coredump discovery.
  @spec diagnose_core_pattern(String.t()) :: String.t() | nil
  def diagnose_core_pattern(pattern) do
    if String.starts_with?(pattern, "|") do
      diagnose_piped_pattern(pattern)
    else
      diagnose_fs_pattern(pattern)
    end
  end

  defp diagnose_piped_pattern(pattern) do
    handler = pipe_handler_name(pattern)

    if not (handler =~ "apport") and System.find_executable("coredumpctl") == nil do
      "Coredump discovery may not work: core_pattern pipes to unknown handler '#{handler}'. " <>
        "Set TOAST_COREDUMP_DIR or --coredump-dir to the directory where your system stores core dumps."
    end
  end

  # kernel core_pattern specifiers per core(5) — %% is handled separately as an
  # escape, %p is substituted with the PID during discovery (see
  # cores_from_fs_pattern/2). Any here-unlisted letter would be left literal
  # and could corrupt the expanded path.
  @core_pattern_specifiers ~r/%[cdeEghiIpPstu]/

  defp diagnose_fs_pattern(pattern) do
    dir =
      pattern
      |> String.replace("%%", "\x00")
      |> String.replace(@core_pattern_specifiers, "*")
      |> String.replace("\x00", "%")
      |> Path.dirname()

    if dir != "." and not File.dir?(dir) do
      "Coredump target directory '#{dir}' does not exist. " <>
        "Core dumps will not be written. Check your kernel core_pattern setting (#{pattern})."
    end
  end

  # --- PID resolution ---

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

  # --- Core file validation ---

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

  # --- Server work directory ---

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

  # --- Override directory ---

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

  # --- /tmp ---

  defp cores_in_tmp(nil), do: []

  defp cores_in_tmp(os_pid) do
    pid_str = to_string(os_pid)

    "/tmp/core*"
    |> Path.wildcard()
    |> Enum.filter(&filename_contains_pid?(&1, pid_str))
  end

  # --- core_pattern ---

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

  defp cores_from_fs_pattern(pattern, os_pid) do
    expanded =
      pattern
      |> String.replace("%%", "\x00")
      |> String.replace("%p", to_string(os_pid || "*"))
      |> String.replace(@core_pattern_specifiers, "*")
      |> String.replace("\x00", "%")

    Logger.debug("Coredump: searching fs pattern #{expanded}")

    if String.contains?(expanded, "/") do
      Path.wildcard(expanded)
    else
      []
    end
  end

  # --- Piped handlers (apport, coredumpctl) ---

  defp pipe_handler_name(pattern) do
    pattern |> String.trim_leading("|") |> String.split() |> hd() |> Path.basename()
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

    case Toast.Diagnostics.Coredump.cmd_with_timeout(
           "apport-unpack",
           [crash_file, dest_dir],
           10_000
         ) do
      {:ok, _, 0} ->
        core_path = Path.join(dest_dir, "CoreDump")
        if File.exists?(core_path), do: [core_path], else: []

      _ ->
        Logger.warning("Failed to unpack apport crash report: #{crash_file}")
        []
    end
  end

  defp extract_coredumpctl_path(os_pid) do
    case Toast.Diagnostics.Coredump.cmd_with_timeout(
           "coredumpctl",
           ["info", to_string(os_pid)],
           5_000
         ) do
      {:ok, output, 0} ->
        case Regex.run(~r/Storage:\s+(\S+)/, output) do
          [_, path] -> [String.trim(path)]
          _ -> []
        end

      _ ->
        []
    end
  end

  # --- Shared helpers ---

  defp filename_contains_pid?(path, pid_str) do
    path |> Path.basename() |> String.split(~r/[.\-_]/) |> Enum.member?(pid_str)
  end
end
