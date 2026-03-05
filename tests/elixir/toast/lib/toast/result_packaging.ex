defmodule Toast.ResultPackaging do
  @moduledoc """
  Tiered result packaging for CI environments.

  Tier 1: Always published (results.json, results.xml, toast.log)
  Tier 2: Compressed archive (server logs, sanitizer reports, crash reports, agency dumps)
  Tier 3: Individually compressed (core dumps)
  """

  require Logger

  @doc "Package results for CI upload. No-op when ci is false."
  @spec package(keyword()) :: :ok
  def package(opts) do
    if Keyword.get(opts, :ci, false) do
      result_dir = Keyword.fetch!(opts, :result_dir)
      File.mkdir_p!(result_dir)

      package_tier1(opts, result_dir)
      package_tier2(opts, result_dir)
      package_tier3(opts, result_dir)
    end

    :ok
  end

  @doc "Compute exit code from aggregated run results."
  @spec exit_code(map()) :: 0 | 1 | 2 | 3 | 4
  def exit_code(results) do
    # Monotonic severity: 4 (crash) > 3 (infra) > 2 (sanitizer) > 1 (test failures) > 0
    cond do
      results.server_crashed -> 4
      results.infrastructure_failure -> 3
      results.sanitizer_errors -> 2
      results.test_failures > 0 -> 1
      true -> 0
    end
  end

  @doc "Determine if zstd is available for compression."
  @spec zstd_available?() :: boolean()
  def zstd_available? do
    System.find_executable("zstd") != nil
  end

  @doc "Determine if gzip is available for compression."
  @spec gzip_available?() :: boolean()
  def gzip_available? do
    System.find_executable("gzip") != nil
  end

  @doc "Compress a file with zstd, falling back to gzip. Returns error if no tool available."
  @spec compress_file(Path.t(), Path.t()) :: {:ok, Path.t()} | {:error, term()}
  def compress_file(source, dest) do
    cond do
      not File.exists?(source) -> {:error, :enoent}
      zstd_available?() -> compress_with_zstd(source, dest)
      gzip_available?() -> compress_with_gzip(source, dest)
      true -> {:error, :no_compression_tool}
    end
  end

  # --- Tier 1: Always published ---

  defp package_tier1(opts, result_dir) do
    # Copy toast.log if a log_file path is provided and differs from result_dir
    case Keyword.get(opts, :log_file) do
      nil ->
        :ok

      log_file ->
        dest = Path.join(result_dir, "toast.log")

        if log_file != dest and File.exists?(log_file) do
          File.cp!(log_file, dest)
        end
    end

    # results.json and results.xml should already be in result_dir from export step
    :ok
  end

  # --- Tier 2: Compressed archive ---

  defp package_tier2(opts, result_dir) do
    suite_diagnostics = Keyword.get(opts, :suite_diagnostics, [])
    files = collect_tier2_files(suite_diagnostics)

    if files != [] do
      archive_path = Path.join(result_dir, "toast-logs.tar.gz")
      create_tar_gz(archive_path, files)
    end

    :ok
  end

  defp collect_tier2_files(suite_diagnostics) do
    Enum.flat_map(suite_diagnostics, fn diag ->
      suite_name = Map.get(diag, :name, "unknown")
      log_files = Map.get(diag, :log_files, [])
      sanitizer_files = Map.get(diag, :sanitizer_files, [])
      crash_reports = Map.get(diag, :crash_reports, [])
      agency_dumps = Map.get(diag, :agency_dumps, [])

      (log_files ++ sanitizer_files ++ crash_reports ++ agency_dumps)
      |> Enum.filter(&File.exists?/1)
      |> Enum.map(fn path -> {suite_name, path} end)
    end)
  end

  defp create_tar_gz(archive_path, tagged_files) do
    tar_files =
      Enum.map(tagged_files, fn {suite_name, path} ->
        archive_name = Path.join(suite_name, Path.basename(path))
        {String.to_charlist(archive_name), String.to_charlist(path)}
      end)

    case :erl_tar.create(String.to_charlist(archive_path), tar_files, [:compressed]) do
      :ok ->
        :ok

      {:error, reason} ->
        Logger.warning("Failed to create archive #{archive_path}: #{inspect(reason)}")
    end
  end

  # --- Tier 3: Individually compressed ---

  defp package_tier3(opts, result_dir) do
    suite_diagnostics = Keyword.get(opts, :suite_diagnostics, [])

    core_dumps =
      Enum.flat_map(suite_diagnostics, fn diag ->
        Map.get(diag, :core_dumps, [])
      end)
      |> Enum.filter(&File.exists?/1)

    Enum.each(core_dumps, fn core_path ->
      basename = Path.basename(core_path)

      case compression_ext() do
        nil ->
          Logger.warning(
            "No compression tool (zstd, gzip) available; copying #{core_path} uncompressed"
          )

          File.cp!(core_path, Path.join(result_dir, basename))

        ext ->
          dest = Path.join(result_dir, basename <> ext)

          case compress_file(core_path, dest) do
            {:ok, _} ->
              :ok

            {:error, reason} ->
              Logger.warning("Failed to compress #{core_path}: #{inspect(reason)}")
          end
      end
    end)

    :ok
  end

  defp compression_ext do
    cond do
      zstd_available?() -> ".zst"
      gzip_available?() -> ".gz"
      true -> nil
    end
  end

  # --- Compression helpers ---

  defp compress_with_zstd(source, dest) do
    case System.cmd("zstd", ["-q", "-f", "-o", dest, source], stderr_to_stdout: true) do
      {_, 0} -> {:ok, dest}
      {output, _} -> {:error, {:zstd_failed, output}}
    end
  end

  defp compress_with_gzip(source, dest) do
    case System.cmd("gzip", ["-c", source], into: File.stream!(dest)) do
      {_, 0} -> {:ok, dest}
      {_, code} -> {:error, {:gzip_failed, code}}
    end
  end
end
