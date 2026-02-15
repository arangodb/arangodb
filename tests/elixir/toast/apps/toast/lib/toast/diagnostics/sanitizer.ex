defmodule Toast.Diagnostics.Sanitizer do
  @moduledoc "Sanitizer environment variable handling and log file collection."

  @type sanitizer_type :: :alubsan | :tsan

  @type sanitizer_error :: %{
          content: String.t(),
          file_path: String.t(),
          timestamp: DateTime.t(),
          sanitizer_type: sanitizer_type(),
          server_id: String.t()
        }

  @alubsan_vars MapSet.new(["ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS"])
  @tsan_vars MapSet.new(["TSAN_OPTIONS"])
  @all_vars MapSet.union(@alubsan_vars, @tsan_vars)

  @doc "Detect active sanitizers from system environment variables."
  @spec detect() :: MapSet.t(String.t())
  def detect do
    Enum.reduce(@all_vars, MapSet.new(), fn var, acc ->
      if System.get_env(var), do: MapSet.put(acc, var), else: acc
    end)
  end

  @doc """
  Generate environment variables for a server process with sanitizer log paths.

  Args:
    - active: MapSet of active sanitizer env var names
    - log_dir: directory where sanitizer log files should be written
    - repo_root: repository root for finding suppression files
  """
  @spec build_env(MapSet.t(String.t()), String.t(), String.t()) :: [{String.t(), String.t()}]
  def build_env(active, log_dir, repo_root) do
    if MapSet.size(active) == 0 do
      []
    else
      Enum.map(active, fn san_var ->
        options = parse_existing_options(san_var)
        options = add_log_path(san_var, options, log_dir)
        options = add_suppressions(san_var, options, repo_root)
        {san_var, format_options(options)}
      end)
    end
  end

  @doc "Collect sanitizer errors from a server's log directory after shutdown."
  @spec collect_errors(String.t(), String.t()) :: [sanitizer_error()]
  def collect_errors(log_dir, server_id) do
    alubsan_errors = collect_log_files(log_dir, "alubsan.log", :alubsan, server_id)
    tsan_errors = collect_log_files(log_dir, "tsan.log", :tsan, server_id)
    alubsan_errors ++ tsan_errors
  end

  # --- Private ---

  defp parse_existing_options(san_var) do
    case System.get_env(san_var) do
      nil ->
        %{}

      existing ->
        existing
        |> String.split(":")
        |> Enum.reduce(%{}, fn item, acc ->
          case String.split(item, "=", parts: 2) do
            [key, value] -> Map.put(acc, key, value)
            _ -> acc
          end
        end)
    end
  end

  defp add_log_path(san_var, options, log_dir) do
    log_name = if san_var in @tsan_vars, do: "tsan.log", else: "alubsan.log"
    log_path = Path.join(log_dir, log_name)

    options
    |> Map.put("log_path", log_path)
    |> Map.put("log_exe_name", "true")
  end

  defp add_suppressions(san_var, options, repo_root) do
    # Extract sanitizer name: "ASAN_OPTIONS" -> "asan"
    san_name = san_var |> String.split("_") |> hd() |> String.downcase()
    suppressions_file = Path.join(repo_root, "#{san_name}_arangodb_suppressions.txt")

    if File.exists?(suppressions_file) do
      Map.put(options, "suppressions", suppressions_file)
    else
      options
    end
  end

  defp format_options(options) do
    options
    |> Enum.map(fn {key, value} ->
      safe_value = String.replace(value, ",", "_")
      "#{key}=#{safe_value}"
    end)
    |> Enum.join(":")
  end

  defp collect_log_files(log_dir, base_name, sanitizer_type, server_id) do
    pattern = Path.join(log_dir, "#{base_name}.*")

    pattern
    |> Path.wildcard()
    |> Enum.flat_map(fn file_path ->
      case File.read(file_path) do
        {:ok, content} when byte_size(content) > 10 ->
          timestamp = get_file_mtime(file_path)

          [
            %{
              content: content,
              file_path: file_path,
              timestamp: timestamp,
              sanitizer_type: sanitizer_type,
              server_id: server_id
            }
          ]

        _ ->
          []
      end
    end)
  end

  defp get_file_mtime(file_path) do
    case File.stat(file_path, time: :posix) do
      {:ok, %{mtime: mtime}} -> DateTime.from_unix!(mtime)
      _ -> DateTime.utc_now()
    end
  end
end
