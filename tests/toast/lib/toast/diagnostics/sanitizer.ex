defmodule Toast.Diagnostics.Sanitizer do
  @moduledoc "Sanitizer environment variable handling and log file collection."

  @typedoc "Sanitizer family. :alubsan = combined ASAN + LSAN + UBSAN."
  @type sanitizer_type :: :alubsan | :tsan

  @type sanitizer_error :: %{
          content: String.t(),
          file_path: String.t(),
          timestamp: Toast.timestamp(),
          sanitizer_type: sanitizer_type(),
          server_id: String.t()
        }

  @alubsan_vars MapSet.new(["ASAN_OPTIONS", "LSAN_OPTIONS", "UBSAN_OPTIONS"])
  @tsan_vars MapSet.new(["TSAN_OPTIONS"])
  @all_vars MapSet.union(@alubsan_vars, @tsan_vars)

  # Default options applied when a sanitizer is explicitly requested (not auto-detected).
  # User env vars override these.
  @default_options %{
    "ASAN_OPTIONS" => %{"halt_on_error" => "0", "detect_leaks" => "1"},
    "LSAN_OPTIONS" => %{"halt_on_error" => "0"},
    "UBSAN_OPTIONS" => %{"halt_on_error" => "0", "print_stacktrace" => "1"},
    "TSAN_OPTIONS" => %{"halt_on_error" => "0", "history_size" => "7"}
  }

  @doc """
  Detect active sanitizers.

  When `explicit` is nil, detects from existing system environment variables.
  When `explicit` is `"tsan"` or `"alubsan"`, forces those sanitizer vars active
  regardless of whether the env vars exist.
  """
  @spec detect(atom() | nil) :: MapSet.t(String.t())
  def detect(explicit \\ nil)

  def detect(:tsan), do: @tsan_vars
  def detect(:alubsan), do: @alubsan_vars

  def detect(nil) do
    Enum.reduce(@all_vars, MapSet.new(), fn var, acc ->
      if System.get_env(var), do: MapSet.put(acc, var), else: acc
    end)
  end

  def detect(other) do
    raise ArgumentError, "invalid sanitizer: #{inspect(other)}, expected :tsan or :alubsan"
  end

  @doc """
  Infer sanitizer type from the build directory path.

  Returns `"alubsan"` if the path contains "asan", `"tsan"` if it contains "tsan",
  or `nil` if no sanitizer can be inferred.
  """
  @spec detect_from_build_dir(Path.t() | nil) :: atom() | nil
  def detect_from_build_dir(nil), do: nil

  def detect_from_build_dir(build_dir) do
    dir = String.downcase(build_dir)

    cond do
      String.contains?(dir, "tsan") -> :tsan
      String.contains?(dir, "asan") -> :alubsan
      true -> nil
    end
  end

  @doc """
  Generate environment variables for a server process with sanitizer log paths.

  When `explicit` is non-nil, default sanitizer options are applied as a base
  before overlaying any user-provided env var values.
  """
  @spec build_env(MapSet.t(String.t()), String.t(), String.t(), atom() | nil) ::
          [{String.t(), String.t()}]
  def build_env(active, log_dir, repo_root, explicit \\ nil) do
    if Enum.empty?(active) do
      []
    else
      san_vars =
        Enum.map(active, fn san_var ->
          options =
            san_var
            |> build_base_options(explicit)
            |> merge_env_options(san_var)
            |> add_log_path(san_var, log_dir)
            |> add_suppressions(san_var, repo_root)

          {san_var, format_options(options)}
        end)

      # Tell our __sanitizer_report_error_summary override where to write
      # per-report timestamps.  See arangod/RestServer/SanitizerReportHook.cpp.
      sidecar = {"ARANGODB_SANITIZER_REPORT_LOG", report_log_prefix(log_dir)}
      [sidecar | san_vars]
    end
  end

  @doc """
  Path prefix for per-report timestamp sidecar files.

  The C++ hook appends `.<pid>` to produce the actual filename, mirroring
  the sanitizer runtime's own `log_path` convention.
  """
  @spec report_log_prefix(String.t()) :: String.t()
  def report_log_prefix(log_dir), do: Path.join(log_dir, "sanitizer_reports.log")

  defp build_base_options(_san_var, nil), do: %{}

  defp build_base_options(san_var, _explicit) do
    Map.get(@default_options, san_var, %{})
  end

  defp merge_env_options(base, san_var) do
    case System.get_env(san_var) do
      nil -> base
      existing -> parse_sanitizer_options(existing, base)
    end
  end

  defp parse_sanitizer_options(options_string, base) do
    options_string
    |> String.split(":")
    |> Enum.reduce(base, fn item, acc ->
      case String.split(item, "=", parts: 2) do
        [key, value] -> Map.put(acc, key, value)
        _ -> acc
      end
    end)
  end

  defp add_log_path(options, san_var, log_dir) do
    log_name = if san_var in @tsan_vars, do: "tsan.log", else: "alubsan.log"
    log_path = Path.join(log_dir, log_name)

    options
    |> Map.put("log_path", log_path)
    |> Map.put("log_exe_name", "true")
  end

  defp add_suppressions(options, san_var, repo_root) do
    san_name = san_var |> String.split("_") |> hd() |> String.downcase()
    suppressions_file = Path.join(repo_root, "#{san_name}_arangodb_suppressions.txt")

    if File.exists?(suppressions_file) do
      Map.put(options, "suppressions", suppressions_file)
    else
      options
    end
  end

  defp format_options(options) do
    Enum.map_join(options, ":", fn {key, value} ->
      safe_value = String.replace(value, ",", "_")
      "#{key}=#{safe_value}"
    end)
  end
end
