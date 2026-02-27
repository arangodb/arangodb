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
      --max-cases     - Maximum number of test modules to run concurrently
      --timeout       - Timeout per test in milliseconds
      --max-failures  - Stop after N failures
      --formatter     - Add a custom formatter module
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
    max_cases: :integer,
    timeout: :integer,
    max_failures: :integer,
    formatter: :keep,
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

  # Maps CLI option keys to TOAST_* environment variable names
  @toast_env_map %{
    build_dir: "TOAST_BUILD_DIR",
    work_dir: "TOAST_WORK_DIR",
    result_dir: "TOAST_RESULT_DIR",
    show_server_logs: "TOAST_SHOW_SERVER_LOGS",
    global_timeout: "TOAST_GLOBAL_TIMEOUT",
    test_timeout: "TOAST_TEST_TIMEOUT",
    startup_timeout: "TOAST_STARTUP_TIMEOUT",
    shutdown_timeout: "TOAST_SHUTDOWN_TIMEOUT",
    timeout_factor: "TOAST_TIMEOUT_FACTOR",
    keep_work_dir: "TOAST_KEEP_WORK_DIR",
    sanitizer: "TOAST_SANITIZER",
    cluster_agents: "TOAST_CLUSTER_AGENTS",
    cluster_dbservers: "TOAST_CLUSTER_DBSERVERS",
    cluster_coordinators: "TOAST_CLUSTER_COORDINATORS",
    replication_factor: "TOAST_CLUSTER_REPLICATION_FACTOR",
    no_agency_dump: "TOAST_NO_AGENCY_DUMP",
    ci: "TOAST_CI"
  }

  @impl Mix.Task
  def run(args) do
    {opts, args_rest} = OptionParser.parse!(args, strict: @switches, aliases: @aliases)

    unless opts[:compile] == false do
      Mix.Task.run("compile", args)
    end

    unless opts[:start] == false do
      Mix.Task.run("app.start", args)
    end

    Application.ensure_all_started(:ex_unit)

    {ex_unit_opts, _} = Mix.Tasks.Toast.Helpers.process_opts(opts)
    ExUnit.configure(ex_unit_opts)

    suites_dir = Path.join(File.cwd!(), "suites")

    if File.dir?(suites_dir) and has_suite_structure?(suites_dir) do
      run_suite_mode(args_rest, opts, ex_unit_opts, suites_dir)
    else
      run_legacy_mode(args_rest, opts, ex_unit_opts)
    end
  end

  defp has_suite_structure?(suites_dir) do
    Mix.Tasks.Toast.Helpers.has_suite_structure?(suites_dir)
  end

  ## Suite mode

  defp run_suite_mode(args, opts, ex_unit_opts, suites_dir) do
    ExUnit.start(Keyword.merge(ex_unit_opts, autorun: false))

    {suite_requests, file_filters} = Mix.Tasks.Toast.Helpers.parse_suite_args(args, suites_dir)

    suite_modules = discover_and_compile_suites(suites_dir, suite_requests)

    if suite_modules == [] do
      Mix.raise("No suites found in #{suites_dir}")
    end

    test_filter = opts[:test]

    suite_data =
      Enum.flat_map(suite_modules, fn {suite_module, suite_dir} ->
        {helpers, test_files} = Mix.Tasks.Toast.Helpers.discover_suite_files(suite_dir)
        compile_helpers(helpers)

        {test_files, line_filters} = Mix.Tasks.Toast.Helpers.apply_file_filters(test_files, file_filters, suite_dir)

        if test_files == [] do
          []
        else
          {test_modules, orphans} = load_test_files(test_files, suite_dir)
          warn_orphans(orphans, suite_dir)
          validate_namespaces(test_modules, suite_module, suite_dir)

          test_modules =
            Enum.filter(test_modules, fn mod ->
              function_exported?(mod, :__toast_suite__, 0) and
                mod.__toast_suite__() == suite_module
            end)

          suite_opts = Mix.Tasks.Toast.Helpers.build_suite_opts(test_modules, line_filters, test_filter)
          [{suite_module, test_modules, suite_opts}]
        end
      end)

    config = Toast.Config.load(Mix.Tasks.Toast.Helpers.opts_to_config_list(opts))
    global_opts = build_global_opts(config, ex_unit_opts)
    result = ToastTest.Runner.run_suites(suite_data, global_opts)

    abort_reason = ToastTest.Runner.aborted?()
    has_sanitizer_errors = Mix.Tasks.Toast.Helpers.has_sanitizer_errors?(result.suites)

    run_results = %{
      test_failures: result.stats.failures,
      server_crashed: match?({:crash, _}, abort_reason),
      infrastructure_failure: abort_reason != nil and not match?({:crash, _}, abort_reason),
      sanitizer_errors: has_sanitizer_errors
    }

    if config.ci do
      suite_diagnostics = Mix.Tasks.Toast.Helpers.build_suite_diagnostics(result.suites)

      Toast.ResultPackaging.package(
        ci: true,
        result_dir: config.result_dir,
        work_dir: config.work_dir,
        suite_diagnostics: suite_diagnostics
      )
    end

    exit_code = Toast.ResultPackaging.exit_code(run_results)

    if exit_code > 0 do
      System.at_exit(fn _ -> exit({:shutdown, exit_code}) end)
    end
  end


  defp build_global_opts(config, ex_unit_opts) do
    global_deadline = System.monotonic_time(:millisecond) + config.global_timeout

    ex_unit_opts
    |> Keyword.merge(
      global_deadline: global_deadline,
      deployment_mode: config.deployment_mode,
      timeout_factor: config.timeout_factor,
      build_dir: config.build_dir,
      work_dir: config.work_dir,
      startup_timeout: config.startup_timeout,
      shutdown_timeout: config.shutdown_timeout,
      show_server_logs: config.show_server_logs,
      keep_work_dir: config.keep_work_dir
    )
  end


  ## Suite discovery helpers

  defp discover_and_compile_suites(suites_dir, suite_requests) do
    suite_files = Path.wildcard(Path.join([suites_dir, "*", "suite.ex"]))

    suite_files =
      case suite_requests do
        :all ->
          suite_files

        names ->
          Enum.filter(suite_files, fn path ->
            dir_name = path |> Path.dirname() |> Path.basename()
            dir_name in names
          end)
      end

    Enum.flat_map(suite_files, fn suite_file ->
      compile_single_suite(suite_file)
    end)
  end

  defp compile_single_suite(suite_file) do
    case Kernel.ParallelCompiler.compile([suite_file], return_diagnostics: true) do
      {:ok, modules, _} ->
        suite_modules =
          Enum.filter(modules, fn mod ->
            behaviours = mod.__info__(:attributes)[:behaviour] || []
            ToastTest.Suite in behaviours
          end)

        Enum.map(suite_modules, fn mod ->
          source = mod.__info__(:compile)[:source] |> to_string()
          {mod, Path.dirname(source)}
        end)

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
          Enum.filter(all_exs, fn path ->
            basename = Path.basename(path)
            not String.starts_with?(basename, "test_")
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

  defp validate_namespaces(_test_modules, _suite_module, _suite_dir) do
    # Namespace validation deferred -- requires knowing the suite's root namespace
    :ok
  end


  ## Legacy mode

  defp run_legacy_mode(files, opts, ex_unit_opts) do
    apply_toast_env(opts)

    test_paths = Mix.Project.config()[:test_paths] || default_test_paths()

    for dir <- test_paths do
      helper = Path.join(dir, "test_helper.exs")
      if File.exists?(helper), do: Code.require_file(helper)
    end

    ExUnit.configure(ex_unit_opts)

    test_pattern = Mix.Project.config()[:test_pattern] || "{test_*,*_test}.exs"

    {test_files, test_opts} =
      if files != [], do: ExUnit.Filters.parse_paths(files), else: {test_paths, []}

    matched_files = find_test_files(test_files, test_pattern)

    if matched_files == [] do
      Logger.info("No test files found")
      ExUnit.Server.modules_loaded(true)

      options =
        ExUnit.configuration()
        |> Keyword.merge(ex_unit_opts)
        |> Keyword.merge(test_opts)

      ToastTest.Runner.run(options, nil)
    else
      Logger.info("Loading #{length(matched_files)} test file(s)")

      case Kernel.ParallelCompiler.require(matched_files, return_diagnostics: true) do
        {:ok, _, _} -> :ok
        {:error, _, _} -> exit({:shutdown, 1})
      end

      load_us = ExUnit.Server.modules_loaded(true)

      options =
        ExUnit.configuration()
        |> Keyword.merge(ex_unit_opts)
        |> Keyword.merge(test_opts)

      {stats, _} = ToastTest.Runner.run(options, load_us)

      if stats.failures > 0 or ToastTest.Runner.aborted?() do
        exit_status = Keyword.get(ex_unit_opts, :exit_status, 2)
        System.at_exit(fn _ -> exit({:shutdown, exit_status}) end)
      end
    end
  end

  ## Toast environment

  defp apply_toast_env(opts) do
    cond do
      opts[:cluster] -> System.put_env("TOAST_DEPLOYMENT_MODE", "cluster")
      opts[:single] -> System.put_env("TOAST_DEPLOYMENT_MODE", "single_server")
      true -> :ok
    end

    Enum.each(@toast_env_map, fn {key, var} ->
      case Keyword.fetch(opts, key) do
        {:ok, value} -> System.put_env(var, to_string(value))
        :error -> :ok
      end
    end)
  end


  ## File discovery

  defp find_test_files(paths, pattern) do
    paths
    |> Enum.flat_map(fn path ->
      if File.dir?(path) do
        Path.wildcard(Path.join(path, "**/" <> pattern))
      else
        if File.exists?(path), do: [path], else: []
      end
    end)
    |> Enum.uniq()
  end

  defp default_test_paths do
    if File.dir?("suites"), do: ["suites"], else: []
  end

end
