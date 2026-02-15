defmodule Toast.Utils.Filesystem do
  @moduledoc "Temp directory management and ArangoDB build detection."

  @type server_dirs :: %{
          base_dir: Path.t(),
          data_dir: Path.t(),
          app_dir: Path.t(),
          log_file: Path.t()
        }

  @spec create_server_dirs(Path.t(), String.t()) :: {:ok, server_dirs()} | {:error, term()}
  def create_server_dirs(work_dir, server_id) do
    base_dir = Path.join(work_dir, server_id)
    data_dir = Path.join(base_dir, "data")
    app_dir = Path.join(base_dir, "apps")
    log_file = Path.join(base_dir, "log")

    with :ok <- File.mkdir_p(data_dir),
         :ok <- File.mkdir_p(app_dir) do
      {:ok, %{base_dir: base_dir, data_dir: data_dir, app_dir: app_dir, log_file: log_file}}
    end
  end

  @spec cleanup_server_dirs(Path.t()) :: :ok
  def cleanup_server_dirs(base_dir) do
    File.rm_rf(base_dir)
    :ok
  end

  @spec find_arangod(Path.t() | nil) :: {:ok, Path.t()} | {:error, String.t()}
  def find_arangod(nil) do
    case System.find_executable("arangod") do
      nil -> {:error, "arangod not found in PATH"}
      path -> {:ok, path}
    end
  end

  def find_arangod(bin_dir) do
    path = Path.join(bin_dir, "arangod")

    if File.exists?(path),
      do: {:ok, path},
      else: {:error, "arangod not found at #{path}"}
  end

  @spec find_repository_root(Path.t() | nil, keyword()) :: {:ok, Path.t()} | {:error, String.t()}
  def find_repository_root(bin_dir, opts \\ []) do
    with :no_match <- find_from_bin_dir(bin_dir),
         :no_match <- find_from_cwd(opts) do
      {:error, "repository root not found"}
    end
  end

  defp find_from_bin_dir(nil), do: :no_match

  defp find_from_bin_dir(bin_dir) do
    candidate =
      if Path.basename(bin_dir) == "bin" do
        bin_dir |> Path.dirname() |> Path.dirname()
      else
        Path.dirname(bin_dir)
      end

    if repository_root?(candidate),
      do: {:ok, candidate},
      else: :no_match
  end

  defp find_from_cwd(opts) do
    Keyword.get_lazy(opts, :cwd, &File.cwd!/0)
    |> walk_up()
    |> Enum.find_value(:no_match, fn dir ->
      if repository_root?(dir), do: {:ok, dir}
    end)
  end

  defp walk_up(path) do
    Stream.unfold(path, fn
      nil -> nil
      dir ->
        parent = Path.dirname(dir)
        if parent == dir, do: {dir, nil}, else: {dir, parent}
    end)
  end

  @doc false
  @spec read_file_or_nil(Path.t() | nil) :: String.t() | nil
  def read_file_or_nil(nil), do: nil

  def read_file_or_nil(path) do
    case File.read(path) do
      {:ok, content} -> content
      {:error, _} -> nil
    end
  end

  defp repository_root?(path) do
    File.dir?(Path.join(path, "js")) and File.dir?(Path.join(path, "etc"))
  end
end
