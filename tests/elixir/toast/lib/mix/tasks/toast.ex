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
      mix toast test/my_test.exs --trace
      mix toast --exclude slow

  ## Toast Options

      --build-dir PATH            - Path to ArangoDB build directory
      --work-dir PATH             - Temporary directory for server data/logs
      --result-dir PATH           - Output directory for test results
      --cluster                   - Use cluster deployment (default: single server)
      --single                    - Use single server deployment (explicit default)
      --show-server-logs          - Print arangod output to stdout
      --global-timeout MS         - Global timeout in milliseconds (default: 3600000)
      --test-timeout MS           - Per-test timeout in milliseconds (default: 300000)
      --startup-timeout MS        - Server startup timeout in milliseconds (default: 60000)
      --shutdown-timeout MS       - Server shutdown timeout in milliseconds (default: 60000)
      --timeout-factor N          - Timeout multiplier (default: 1, auto-set to 3 for sanitizer builds)
      --keep-work-dir             - Keep server data/logs even on success
      --sanitizer TYPE            - Sanitizer: tsan or alubsan (auto-detected from build dir)
      --cluster-agents N          - Number of agency nodes (default: 3)
      --cluster-dbservers N       - Number of DB servers (default: 3)
      --cluster-coordinators N    - Number of coordinators (default: 1)
      --replication-factor N      - Default replication factor (default: 2)

  ## ExUnit Options

      --include       - Include tests matching the filter
      --exclude       - Exclude tests matching the filter
      --only          - Run only tests matching the filter (excludes all others)
      --trace         - Enable verbose output
      --timeout       - Timeout per test in milliseconds
      --max-failures  - Stop after N failures
      --color         - Enable ANSI coloring
      --no-color      - Disable ANSI coloring
      --no-compile    - Skip project compilation
      --no-start      - Skip application startup

  All Toast options can also be set via `TOAST_*` environment variables.
  CLI arguments take precedence over environment variables.
  """

  use Mix.Task

  require Logger

  @compile {:no_warn_undefined, [ExUnit, ExUnit.Filters]}

  @preferred_cli_env :test

  @switches [
    # ExUnit options
    include: :keep,
    exclude: :keep,
    only: :keep,
    trace: :boolean,
    timeout: :integer,
    max_failures: :integer,
    color: :boolean,
    compile: :boolean,
    start: :boolean,
    # Toast options
    build_dir: :string,
    work_dir: :string,
    result_dir: :string,
    cluster: :boolean,
    single: :boolean,
    show_server_logs: :boolean,
    global_timeout: :integer,
    test_timeout: :integer,
    startup_timeout: :integer,
    shutdown_timeout: :integer,
    timeout_factor: :integer,
    keep_work_dir: :boolean,
    sanitizer: :string,
    cluster_agents: :integer,
    cluster_dbservers: :integer,
    cluster_coordinators: :integer,
    replication_factor: :integer,
    test: :string,
    no_agency_dump: :boolean,
    ci: :boolean
  ]

  @aliases [
    i: :include,
    e: :exclude,
    t: :trace,
    b: :build_dir
  ]

  @impl Mix.Task
  def run(args) do
    {opts, args_rest} = OptionParser.parse!(args, strict: @switches, aliases: @aliases)

    unless opts[:compile] == false do
      Mix.Task.run("compile", [])
    end

    unless opts[:start] == false do
      Mix.Task.run("app.start", [])
    end

    Application.ensure_all_started(:ex_unit)

    ex_unit_opts = Mix.Tasks.Toast.Helpers.process_opts(opts)
    ExUnit.configure(ex_unit_opts)

    suites_dir = Path.join(File.cwd!(), "suites")
    run_suite_mode(args_rest, opts, ex_unit_opts, suites_dir)
  end

  defp run_suite_mode(args, opts, ex_unit_opts, suites_dir) do
    ExUnit.start(Keyword.merge(ex_unit_opts, autorun: false))

    {suite_requests, file_filters} = Mix.Tasks.Toast.Helpers.parse_suite_args(args)

    suite_modules = discover_and_compile_suites(suites_dir, suite_requests)

    if suite_modules == [] do
      Mix.raise("No suites found in #{suites_dir}")
    end

    test_filter = opts[:test]

    suite_data =
      Enum.flat_map(suite_modules, fn {suite_module, suite_dir} ->
        prepare_suite(suite_module, suite_dir, file_filters, test_filter)
      end)

    config = Toast.Config.load(Mix.Tasks.Toast.Helpers.opts_to_config_list(opts))
    Toast.Application.reconfigure_file_logger(config.result_dir)
    global_opts = build_global_opts(config, ex_unit_opts)
    result = ToastTest.Runner.run_suites(suite_data, global_opts)

    abort_reason = ToastTest.Abort.reason()
    has_sanitizer_errors = Toast.Diagnostics.Summary.has_sanitizer_errors?(result.suites)

    run_results = %{
      test_failures: result.stats.failures,
      server_crashed: match?({:crash, _}, abort_reason),
      infrastructure_failure: abort_reason != nil and not match?({:crash, _}, abort_reason),
      sanitizer_errors: has_sanitizer_errors
    }

    if config.ci do
      suite_diagnostics = Toast.Diagnostics.Summary.build_suite_diagnostics(result.suites)

      Toast.ResultPackaging.package(
        ci: true,
        result_dir: config.result_dir,
        suite_diagnostics: suite_diagnostics
      )
    end

    exit_code = Toast.ResultPackaging.exit_code(run_results)

    if exit_code > 0 do
      System.at_exit(fn _ -> exit({:shutdown, exit_code}) end)
    end
  end

  defp prepare_suite(suite_module, suite_dir, file_filters, test_filter) do
    {helpers, test_files} = Mix.Tasks.Toast.Helpers.discover_suite_files(suite_dir)
    compile_helpers(helpers)

    {test_files, line_filters} =
      Mix.Tasks.Toast.Helpers.apply_file_filters(test_files, file_filters, suite_dir)

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

    suite_opts = Mix.Tasks.Toast.Helpers.build_suite_opts(test_modules, line_filters, test_filter)
    [{suite_module, test_modules, suite_opts}]
  end

  defp build_global_opts(config, ex_unit_opts) do
    global_deadline = System.monotonic_time(:millisecond) + config.global_timeout

    mode_exclusion =
      case config.deployment_mode do
        :cluster -> [:single_only]
        :single_server -> [:cluster_only]
      end

    ex_unit_opts
    |> Keyword.update(:exclude, mode_exclusion, &(mode_exclusion ++ &1))
    |> Keyword.merge(
      global_deadline: global_deadline,
      deployment_mode: config.deployment_mode,
      timeout: config.test_timeout,
      timeout_factor: config.timeout_factor,
      build_dir: config.build_dir,
      work_dir: config.work_dir,
      startup_timeout: config.startup_timeout,
      shutdown_timeout: config.shutdown_timeout,
      show_server_logs: config.show_server_logs,
      keep_work_dir: config.keep_work_dir,
      cluster_agents: config.cluster_agents,
      cluster_dbservers: config.cluster_dbservers,
      cluster_coordinators: config.cluster_coordinators,
      replication_factor: config.cluster_replication_factor
    )
  end

  ## Suite discovery helpers

  defp discover_and_compile_suites(suites_dir, suite_requests) do
    suite_files =
      [suites_dir, "*", "suite.ex"]
      |> Path.join()
      |> Path.wildcard()
      |> filter_by_request(suite_requests)

    Enum.flat_map(suite_files, &compile_single_suite/1)
  end

  defp filter_by_request(suite_files, :all), do: suite_files

  defp filter_by_request(suite_files, names) do
    Enum.filter(suite_files, fn path ->
      path |> Path.dirname() |> Path.basename() |> Kernel.in(names)
    end)
  end

  defp compile_single_suite(suite_file) do
    case Kernel.ParallelCompiler.compile([suite_file], return_diagnostics: true) do
      {:ok, modules, _} ->
        for mod <- modules,
            ToastTest.Suite in (mod.__info__(:attributes)[:behaviour] || []) do
          source = mod.__info__(:compile)[:source] |> to_string()
          {mod, Path.dirname(source)}
        end

      {:error, errors, _} ->
        Logger.error("Failed to compile suite #{suite_file}: #{inspect(errors)}")
        []
    end
  end

  defp compile_helpers([]), do: :ok

  defp compile_helpers(helpers) do
    case Kernel.ParallelCompiler.compile(helpers, return_diagnostics: true) do
      {:ok, _, _} -> :ok
      {:error, errors, _} -> Logger.error("Failed to compile helpers: #{inspect(errors)}")
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

      {:error, errors, _} ->
        Logger.error("Failed to load test files: #{inspect(errors)}")
        {[], []}
    end
  end

  defp warn_orphans([], _suite_dir), do: :ok

  defp warn_orphans(orphans, suite_dir) do
    for path <- orphans do
      relative = Path.relative_to(path, Path.dirname(suite_dir))

      Logger.warning(
        "#{relative} is not a test file (must start with test_) and is not compiled as a helper (must end in .ex). This file is ignored."
      )
    end
  end
end
