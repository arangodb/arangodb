defmodule Toast.Utils.Filesystem do
  @moduledoc "Temp directory management and ArangoDB build detection."

  require Logger

  @type server_dirs :: %{
          base_dir: Path.t(),
          data_dir: Path.t(),
          app_dir: Path.t(),
          log_file: Path.t()
        }

  @spec create_server_dirs(Path.t()) :: {:ok, server_dirs()} | {:error, term()}
  def create_server_dirs(dir) do
    data_dir = Path.join(dir, "data")
    app_dir = Path.join(dir, "apps")
    log_file = Path.join(dir, "log")

    with :ok <- File.mkdir_p(data_dir),
         :ok <- File.mkdir_p(app_dir) do
      {:ok, %{base_dir: dir, data_dir: data_dir, app_dir: app_dir, log_file: log_file}}
    end
  end

  @spec create_server_dirs(Path.t(), String.t()) :: {:ok, server_dirs()} | {:error, term()}
  def create_server_dirs(deployment_dir, server_id) do
    create_server_dirs(Path.join(deployment_dir, server_id))
  end

  @spec find_arangod(Path.t() | nil) :: {:ok, Path.t()} | {:error, String.t()}
  def find_arangod(nil) do
    case System.find_executable("arangod") do
      nil ->
        Logger.debug("arangod not found in PATH")
        {:error, "arangod not found in PATH"}

      path ->
        Logger.debug("Found arangod in PATH: #{path}")
        {:ok, path}
    end
  end

  def find_arangod(build_dir) do
    path = Path.join([Path.expand(build_dir), "bin", "arangod"])

    if File.exists?(path) do
      Logger.debug("Found arangod: #{path}")
      {:ok, path}
    else
      Logger.debug("arangod not found at #{path}")
      {:error, "arangod not found at #{path}"}
    end
  end

  @spec find_repository_root(Path.t() | nil, keyword()) :: {:ok, Path.t()} | {:error, String.t()}
  def find_repository_root(build_dir, opts \\ []) do
    with :no_match <- find_from_build_dir(build_dir),
         :no_match <- find_from_cwd(opts) do
      Logger.debug("Repository root not found")
      {:error, "repository root not found"}
    else
      {:ok, root} = result ->
        Logger.debug("Repository root: #{root}")
        result
    end
  end

  defp find_from_build_dir(nil), do: :no_match

  defp find_from_build_dir(build_dir) do
    candidate = build_dir |> Path.expand() |> Path.dirname()

    if repository_root?(candidate),
      do: {:ok, candidate},
      else: :no_match
  end

  defp find_from_cwd(opts) do
    opts
    |> Keyword.get_lazy(:cwd, &File.cwd!/0)
    |> walk_up()
    |> Enum.find_value(:no_match, fn dir ->
      if repository_root?(dir), do: {:ok, dir}
    end)
  end

  defp walk_up(path) do
    Stream.unfold(path, fn
      nil ->
        nil

      dir ->
        parent = Path.dirname(dir)
        if parent == dir, do: {dir, nil}, else: {dir, parent}
    end)
  end

  defp repository_root?(path) do
    # Check for ArangoDB-specific directories to avoid false positives.
    # "arangod/" is the server source dir — unlikely to exist elsewhere.
    File.dir?(Path.join(path, "arangod")) and
      File.dir?(Path.join(path, "js")) and
      File.dir?(Path.join(path, "etc"))
  end

  @spec file_mtime_us(Path.t()) :: {:ok, integer()} | {:error, term()}
  def file_mtime_us(path) do
    case System.cmd("stat", ["-c", "%y", path], stderr_to_stdout: true) do
      {output, 0} -> parse_stat_mtime(output, path)
      _ -> file_mtime_fallback(path)
    end
  rescue
    _ -> file_mtime_fallback(path)
  end

  defp parse_stat_mtime(output, path) do
    case Regex.run(
           ~r/^(\d{4}-\d{2}-\d{2}) (\d{2}:\d{2}:\d{2})\.(\d+) ([+-]\d{4})/,
           String.trim(output)
         ) do
      [_, date, time, nanos, offset] ->
        usec_str = nanos |> String.slice(0, 6) |> String.pad_trailing(6, "0")
        <<tz_h::binary-size(3), tz_m::binary>> = offset
        iso = "#{date}T#{time}.#{usec_str}#{tz_h}:#{tz_m}"

        case DateTime.from_iso8601(iso) do
          {:ok, dt, _offset} -> {:ok, DateTime.to_unix(dt, :microsecond)}
          _ -> file_mtime_fallback(path)
        end

      _ ->
        file_mtime_fallback(path)
    end
  end

  defp file_mtime_fallback(path) do
    case File.stat(path, time: :posix) do
      {:ok, stat} -> {:ok, stat.mtime * 1_000_000}
      error -> error
    end
  end
end
