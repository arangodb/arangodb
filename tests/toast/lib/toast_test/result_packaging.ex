defmodule ToastTest.ResultPackaging do
  @moduledoc """
  Tiered result packaging for CI environments.

  Tier 1: Always published (results.json, results.xml, toast.log, agency dumps)
  Tier 2: Compressed archive (server logs, sanitizer reports)
  Tier 3: Large individually compressed files (core dumps, base dir archive)

  Agency dumps are written to result_dir at capture time by post-execution,
  so they are already present when packaging runs.
  """

  require Logger

  @doc """
  Package results for CI upload. No-op when ci is false.

  Tiers are gated on what issues occurred:
  - Tier 1 (always): structured results, toast.log, agency dumps
  - Tier 2 (any failure): server logs, sanitizer reports
  - Tier 3 (server crash): core dumps, work dir archive

  Pass `force_all_tiers: true` to bypass gating and package all tiers
  regardless of outcome.
  """
  @spec package(keyword()) :: :ok
  def package(opts) do
    if Keyword.get(opts, :ci, false) do
      result_dir = Keyword.fetch!(opts, :result_dir)
      run_results = Keyword.fetch!(opts, :run_results)
      force_all = Keyword.get(opts, :force_all_tiers, false)
      File.mkdir_p!(result_dir)

      package_tier1(opts, result_dir)

      if force_all or any_failure?(run_results) do
        package_tier2(opts, result_dir)
      end

      if force_all or run_results.server_crashed do
        package_tier3(opts, result_dir)
      end
    end

    :ok
  end

  defp any_failure?(run_results),
    do: ToastTest.DiagnosticsSummary.exit_code(run_results) > 0

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
    case opts |> Keyword.get(:suite_diagnostics, []) |> collect_tier2_files() do
      [] ->
        :ok

      files ->
        archive_path = Path.join(result_dir, "toast-logs.tar.gz")
        create_tar_gz(archive_path, files)
    end
  end

  defp collect_tier2_files(suite_diagnostics) do
    Enum.flat_map(suite_diagnostics, fn diag ->
      suite_name = Map.get(diag, :name, "unknown")

      # TODO: :crash_reports — will contain internal crash dump artifact paths once
      # the crash dump feature is implemented.
      [:log_files, :sanitizer_files, :crash_reports]
      |> Enum.flat_map(&Map.get(diag, &1, []))
      |> Enum.filter(&File.exists?/1)
      |> Enum.map(&{suite_name, &1})
    end)
  end

  defp create_tar_gz(archive_path, tagged_files) do
    tar_files =
      Enum.map(tagged_files, fn {suite_name, path} ->
        archive_name = Path.join(suite_name, flatten_artifact_name(path))
        {String.to_charlist(archive_name), String.to_charlist(path)}
      end)

    case :erl_tar.create(String.to_charlist(archive_path), tar_files, [:compressed]) do
      :ok ->
        :ok

      {:error, reason} ->
        Logger.warning("Failed to create archive #{archive_path}: #{inspect(reason)}")
    end
  end

  # --- Tier 3: Large individually compressed files ---

  defp package_tier3(opts, result_dir) do
    tool = Toast.Utils.Compression.detect_tool()

    opts
    |> Keyword.get(:suite_diagnostics, [])
    |> Enum.flat_map(&Map.get(&1, :core_dumps, []))
    |> Enum.filter(&File.exists?/1)
    |> Enum.each(&package_core_dump(&1, result_dir, tool))

    package_base_dir(opts, result_dir)
  end

  defp package_base_dir(opts, result_dir) do
    case Keyword.get(opts, :base_dir) do
      nil -> :ok
      base_dir -> archive_base_dir(base_dir, result_dir)
    end
  end

  defp archive_base_dir(base_dir, result_dir) do
    if File.dir?(base_dir) do
      archive_path = Path.join(result_dir, "work-dir.tar.gz")
      Logger.info("Archiving base dir #{base_dir} → #{archive_path}")

      case :erl_tar.create(
             String.to_charlist(archive_path),
             [{String.to_charlist("work-dir"), String.to_charlist(base_dir)}],
             [:compressed, :dereference]
           ) do
        :ok -> :ok
        {:error, reason} -> Logger.warning("Failed to archive work dir: #{inspect(reason)}")
      end
    else
      :ok
    end
  end

  defp package_core_dump(core_path, result_dir, tool) do
    basename = Path.basename(core_path)

    case tool do
      nil ->
        Logger.warning(
          "No compression tool (zstd, gzip) available; copying #{core_path} uncompressed"
        )

        File.cp!(core_path, Path.join(result_dir, basename))

      :zstd ->
        dest = Path.join(result_dir, basename <> ".zst")
        compress_core(core_path, dest, &Toast.Utils.Compression.compress_with_zstd/2)

      :gzip ->
        dest = Path.join(result_dir, basename <> ".gz")
        compress_core(core_path, dest, &Toast.Utils.Compression.compress_with_gzip/2)
    end
  end

  defp compress_core(core_path, dest, compress_fn) do
    case compress_fn.(core_path, dest) do
      {:ok, _} ->
        :ok

      {:error, reason} ->
        Logger.warning("Failed to compress #{core_path}: #{inspect(reason)}")
    end
  end

  # Flattens "deployment_dir/dbserver-0/log" → "dbserver-0.log"
  # and "deployment_dir/dbserver-0/tsan.log" → "dbserver-0.tsan.log"
  defp flatten_artifact_name(path) do
    server_id = path |> Path.dirname() |> Path.basename()
    basename = Path.basename(path)
    "#{server_id}.#{basename}"
  end
end
