defmodule Mix.Tasks.Toast do
  @shortdoc "Runs Toast test suites"
  @moduledoc """
  Runs Toast test suites using `ToastTest.Runner` instead of ExUnit's built-in runner.

  ## Usage

      mix toast [options] [files]

  ## Examples

      mix toast --build-dir ../build-clang
      mix toast --cluster --cluster-dbservers 2
      mix toast --sanitizer alubsan
      mix toast --exclude slow

  ## Toast Options

      --build-dir PATH            - Path to ArangoDB build directory
      --base-dir PATH             - Base directory for server data/logs
      --result-dir PATH           - Output directory for test results
      --cluster                   - Use cluster deployment (default: single server)
      --single                    - Use single server deployment (explicit default)
      --show-server-logs          - Print arangod output to stdout
      --global-timeout MS         - Global timeout in milliseconds (default: 3600000)
      --test-timeout MS           - Per-test timeout in milliseconds (default: 300000)
      --startup-timeout MS        - Server startup timeout in milliseconds (default: 60000)
      --shutdown-timeout MS       - Server shutdown timeout in milliseconds (default: 60000)
      --timeout-factor N          - Timeout multiplier (default: 1, auto-set to 3 for sanitizer builds)
      --keep-data                 - Keep server data/logs even on success
      --sanitizer TYPE            - Sanitizer: tsan or alubsan (auto-detected from build dir)
      --attach-debugger           - Pause after deployment for live debugger attachment (disables test timeouts)
      --http2                     - Use HTTP/2 (h2c) for client requests (default: HTTP/1.1)
      --rr ROLES                  - Record with rr: "default", "all", or comma-separated roles
                                    (single, agent, dbserver, coordinator)
                                    "default" = single server or dbserver,coordinator in cluster
      --cluster-agents N          - Number of agency nodes (default: 3)
      --cluster-dbservers N       - Number of DB servers (default: 3)
      --cluster-coordinators N    - Number of coordinators (default: 1)
      --replication-factor N      - Default replication factor (default: 2)

  ## Bucketing Options

      --test-buckets TOTAL/INDEX    - Run only bucket INDEX of TOTAL (0-indexed).
                                      Modules are partitioned by weight for balanced
                                      parallel CI execution. Set weight per test module
                                      via `use YourSuite, weight: N` (default: 1).

  ## CI Options

      --ci                        - Enable CI mode (packages results into tiers for upload)
      --force-all-tiers           - Package all tiers regardless of outcome (CI only)
      --no-agency-dump            - Skip agency state dump on error

  ## Filtering Options

      --include TAG               - Include tests matching the filter
      --exclude TAG               - Exclude tests matching the filter
      --only TAG                  - Run only tests matching the filter (excludes all others)
      --max-failures N            - Stop after N failures
      --test PATTERN              - Run only tests whose name contains PATTERN

  All Toast options can also be set via `TOAST_*` environment variables.
  CLI arguments take precedence over environment variables.
  """

  use Mix.Task

  require Logger

  alias Mix.Tasks.Toast.Helpers
  alias ToastTest.DiagnosticsSummary
  alias ToastTest.ResultPackaging

  @compile {:no_warn_undefined, [ExUnit, ExUnit.Filters]}

  @switches [
    # Filtering options
    include: :keep,
    exclude: :keep,
    only: :keep,
    max_failures: :integer,
    # Toast options
    build_dir: :string,
    base_dir: :string,
    result_dir: :string,
    cluster: :boolean,
    single: :boolean,
    show_server_logs: :boolean,
    global_timeout: :integer,
    test_timeout: :integer,
    startup_timeout: :integer,
    shutdown_timeout: :integer,
    timeout_factor: :integer,
    memory_budget: :integer,
    keep_data: :boolean,
    sanitizer: :string,
    attach_debugger: :boolean,
    rr: :string,
    http2: :boolean,
    cluster_agents: :integer,
    cluster_dbservers: :integer,
    cluster_coordinators: :integer,
    replication_factor: :integer,
    test: :string,
    test_buckets: :string,
    no_agency_dump: :boolean,
    ci: :boolean,
    force_all_tiers: :boolean,
    help: :boolean
  ]

  @aliases [
    i: :include,
    e: :exclude
  ]

  @impl Mix.Task
  def run(args) do
    {opts, args_rest} = OptionParser.parse!(args, strict: @switches, aliases: @aliases)

    if opts[:help] || args_rest == ["help"] do
      print_help()
    else
      Mix.Task.run("compile", [])

      opts |> Helpers.opts_to_env_list() |> Toast.Env.load() |> Toast.Env.apply!()

      Mix.Task.run("app.start", [])

      Application.ensure_all_started(:ex_unit)

      ex_unit_opts = Helpers.process_opts(opts)
      ExUnit.configure(ex_unit_opts)

      suites_dir = Path.join(File.cwd!(), "suites")
      run_suite_mode(args_rest, opts, ex_unit_opts, suites_dir)
    end
  end

  defp print_help do
    Mix.shell().info(@moduledoc)
  end

  defp run_suite_mode(args, opts, ex_unit_opts, suites_dir) do
    ExUnit.start(Keyword.merge(ex_unit_opts, autorun: false))

    args = Helpers.expand_args(args, suites_dir)
    {suite_requests, file_filters} = Helpers.parse_suite_args(args)

    suite_modules = discover_and_compile_suites(suites_dir, suite_requests)

    if suite_modules == [] do
      Mix.raise("No suites found in #{suites_dir}")
    end

    test_filter = opts[:test]

    suite_data =
      Enum.flat_map(suite_modules, fn {suite_module, suite_dir} ->
        prepare_suite(suite_module, suite_dir, file_filters, test_filter)
      end)

    suite_data = apply_bucket_filter(suite_data, opts[:test_buckets])

    test_config = ToastTest.Config.new()
    Toast.Application.reconfigure_file_logger(test_config.result_dir)
    start_time = System.monotonic_time()
    result = ToastTest.Runner.run_suites(suite_data, test_config, ex_unit_opts)

    elapsed_us =
      System.convert_time_unit(System.monotonic_time() - start_time, :native, :microsecond)

    suite_results = Enum.map(result.suites, & &1.suite_result)
    Enum.each(suite_results, &ToastTest.Formatting.PostExecSummary.print/1)
    ToastTest.Formatting.RunSummary.print(suite_results, elapsed_us)
    ToastTest.Formatting.RrSummary.print(test_config.base_dir)

    run_results = %{
      test_failures: result.stats.failures,
      server_crashed: DiagnosticsSummary.has_server_crash?(result.suites),
      infrastructure_failure: DiagnosticsSummary.has_timeout?(result.suites),
      sanitizer_errors: DiagnosticsSummary.has_sanitizer_errors?(result.suites)
    }

    if test_config.ci do
      suite_diagnostics = DiagnosticsSummary.build_suite_diagnostics(result.suites)

      ResultPackaging.package(
        ci: true,
        run_results: run_results,
        force_all_tiers: test_config.force_all_tiers,
        result_dir: test_config.result_dir,
        base_dir: test_config.base_dir,
        suite_diagnostics: suite_diagnostics
      )
    end

    exit_code = DiagnosticsSummary.exit_code(run_results)

    if exit_code > 0 do
      System.at_exit(fn _ -> exit({:shutdown, exit_code}) end)
    end
  end

  defp prepare_suite(suite_module, suite_dir, file_filters, test_filter) do
    {helpers, test_files} = Helpers.discover_suite_files(suite_dir)
    compile_helpers(helpers)

    {test_files, line_filters} =
      Helpers.apply_file_filters(test_files, file_filters, suite_dir)

    case test_files do
      [] -> []
      _ -> build_suite_entry(suite_module, suite_dir, test_files, line_filters, test_filter)
    end
  end

  defp build_suite_entry(suite_module, suite_dir, test_files, line_filters, test_filter) do
    {test_modules, orphans} = load_test_files(test_files, suite_dir)
    warn_orphans(orphans, suite_dir)

    test_modules =
      Enum.filter(test_modules, fn mod ->
        function_exported?(mod, :__toast_suite__, 0) and mod.__toast_suite__() == suite_module
      end)

    suite_opts = Helpers.build_suite_opts(test_modules, line_filters, test_filter)
    suite_name = Path.basename(suite_dir)
    [{suite_module, test_modules, suite_opts, suite_name}]
  end

  ## Suite discovery helpers

  defp discover_and_compile_suites(suites_dir, suite_requests) do
    [suites_dir, "*", "suite.ex"]
    |> Path.join()
    |> Path.wildcard()
    |> filter_by_request(suite_requests)
    |> Enum.flat_map(&compile_single_suite/1)
  end

  defp filter_by_request(suite_files, :all), do: suite_files

  defp filter_by_request(suite_files, names) do
    Enum.filter(suite_files, fn path ->
      suite_name = path |> Path.dirname() |> Path.basename()
      suite_name in names
    end)
  end

  defp compile_single_suite(suite_file) do
    case Kernel.ParallelCompiler.compile([suite_file], return_diagnostics: true) do
      {:ok, modules, _} ->
        for mod <- modules,
            ToastTest.Suite in behaviours(mod) do
          source = mod.__info__(:compile)[:source] |> to_string()
          {mod, Path.dirname(source)}
        end

      {:error, _errors, _} ->
        Mix.raise("Failed to compile suite #{suite_file}")
    end
  end

  defp behaviours(mod), do: mod.__info__(:attributes)[:behaviour] || []

  defp compile_helpers([]), do: :ok

  defp compile_helpers(helpers) do
    case Kernel.ParallelCompiler.compile(helpers, return_diagnostics: true) do
      {:ok, _, _} -> :ok
      {:error, _errors, _} -> Mix.raise("Failed to compile test helpers")
    end
  end

  defp load_test_files(test_files, suite_dir) do
    case Kernel.ParallelCompiler.require(test_files, return_diagnostics: true) do
      {:ok, modules, _} ->
        all_exs = Path.wildcard(Path.join(suite_dir, "*.exs"))

        orphans =
          Enum.reject(all_exs, fn path ->
            path |> Path.basename() |> String.starts_with?("test_")
          end)

        {modules, orphans}

      {:error, _errors, _} ->
        Mix.raise("Failed to compile test files in #{suite_dir}")
    end
  end

  defp warn_orphans([], _suite_dir), do: :ok

  defp warn_orphans(orphans, suite_dir) do
    Enum.each(orphans, fn path ->
      relative = Path.relative_to(path, Path.dirname(suite_dir))

      Logger.warning(
        "#{relative} is not a test file (must start with test_) and is not compiled as a helper (must end in .ex). This file is ignored."
      )
    end)
  end

  ## Bucketing helpers

  defp apply_bucket_filter(suite_data, nil), do: suite_data

  defp apply_bucket_filter(suite_data, spec) do
    case ToastTest.Bucket.parse_spec(spec) do
      {:ok, {total, index}} -> filter_to_bucket(suite_data, total, index)
      {:error, reason} -> Mix.raise(reason)
    end
  end

  defp filter_to_bucket(suite_data, total, index) do
    all_modules =
      Enum.flat_map(suite_data, fn {_suite_mod, test_modules, _opts, suite_name} ->
        Enum.map(test_modules, fn mod -> {suite_name, mod} end)
      end)

    selected = ToastTest.Bucket.select(all_modules, index, total, &ToastTest.Suite.weight/1)
    selected_set = MapSet.new(selected, fn {_suite, mod} -> mod end)

    suite_data
    |> Enum.map(fn {suite_mod, test_modules, opts, suite_name} ->
      filtered = Enum.filter(test_modules, &MapSet.member?(selected_set, &1))
      {suite_mod, filtered, opts, suite_name}
    end)
    |> Enum.reject(fn {_, test_modules, _, _} -> test_modules == [] end)
  end
end
