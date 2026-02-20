defmodule Mix.Tasks.Toast do
  @shortdoc "Runs Toast test suites"
  @moduledoc """
  Runs Toast test suites using `Toast.Runner` instead of ExUnit's built-in runner.

  ## Usage

      mix toast [options] [files]

  ## Examples

      mix toast --build-dir ../build-clang
      mix toast --cluster --cluster-dbservers 2
      mix toast --sanitizer alubsan
      mix toast test/my_test.exs --trace
      mix toast --exclude slow --seed 12345

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
      --seed          - Seed for test randomization (0 disables shuffling)
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
  @recursive true

  @switches [
    # ExUnit options
    include: :keep,
    exclude: :keep,
    only: :keep,
    seed: :integer,
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
    replication_factor: :integer
  ]

  @aliases [
    i: :include,
    e: :exclude,
    s: :seed,
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
    replication_factor: "TOAST_CLUSTER_REPLICATION_FACTOR"
  }

  # Keys passed through to ExUnit.configure
  @option_keys [
    :include,
    :exclude,
    :seed,
    :trace,
    :max_cases,
    :timeout,
    :max_failures,
    :formatters,
    :colors,
    :exit_status,
    :only_test_ids
  ]

  @impl Mix.Task
  def run(args) do
    {opts, files} = OptionParser.parse!(args, strict: @switches, aliases: @aliases)

    unless opts[:compile] == false do
      Mix.Task.run("compile", args)
    end

    unless opts[:start] == false do
      Mix.Task.run("app.start", args)
    end

    Application.ensure_all_started(:ex_unit)

    apply_toast_env(opts)

    {ex_unit_opts, _allowed_files} = process_opts(opts)
    ExUnit.configure(ex_unit_opts)

    # Load test_helper.exs — this calls ExUnit.start() and setup_suite()
    test_paths = Mix.Project.config()[:test_paths] || default_test_paths()

    for dir <- test_paths do
      helper = Path.join(dir, "test_helper.exs")
      if File.exists?(helper), do: Code.require_file(helper)
    end

    # Re-apply CLI opts so they override anything test_helper.exs configured
    ExUnit.configure(ex_unit_opts)

    test_pattern = Mix.Project.config()[:test_pattern] || "*_test.exs"

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

      Toast.Runner.run(options, nil)
    else
      Logger.info("Loading #{length(matched_files)} test file(s)")

      seed = Application.get_env(:ex_unit, :seed) || 0
      rand_algorithm = Application.get_env(:ex_unit, :rand_algorithm) || :exsss
      matched_files = shuffle(seed, rand_algorithm, matched_files)

      case Kernel.ParallelCompiler.require(matched_files, return_diagnostics: true) do
        {:ok, _, _} -> :ok
        {:error, _, _} -> exit({:shutdown, 1})
      end

      load_us = ExUnit.Server.modules_loaded(true)

      options =
        ExUnit.configuration()
        |> Keyword.merge(ex_unit_opts)
        |> Keyword.merge(test_opts)

      {stats, _} = Toast.Runner.run(options, load_us)

      if stats.failures > 0 or Toast.Runner.aborted?() do
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

  ## Option processing

  defp process_opts(opts) do
    ex_unit_opts =
      opts
      |> filter_opts(:include)
      |> filter_opts(:exclude)
      |> filter_only()
      |> formatter_opts()
      |> color_opts()
      |> Keyword.put_new(:exit_status, 2)
      |> Keyword.take(@option_keys)

    {[autorun: false] ++ ex_unit_opts, nil}
  end

  defp filter_opts(opts, key) do
    case Keyword.get_values(opts, key) do
      [] ->
        opts

      values ->
        parsed = ExUnit.Filters.parse(values)
        opts |> Keyword.delete(key) |> Keyword.put(key, parsed)
    end
  end

  defp filter_only(opts) do
    case Keyword.get_values(opts, :only) do
      [] ->
        opts

      values ->
        parsed = ExUnit.Filters.parse(values)

        opts
        |> Keyword.delete(:only)
        |> Keyword.update(:include, parsed, &(parsed ++ &1))
        |> Keyword.update(:exclude, [:test], &[:test | &1])
    end
  end

  defp formatter_opts(opts) do
    case Keyword.get_values(opts, :formatter) do
      [] ->
        Keyword.delete(opts, :formatter)

      formatters ->
        modules = Enum.map(formatters, &Module.concat([&1]))
        opts |> Keyword.delete(:formatter) |> Keyword.put(:formatters, modules)
    end
  end

  defp color_opts(opts) do
    case Keyword.fetch(opts, :color) do
      {:ok, enabled?} ->
        opts |> Keyword.delete(:color) |> Keyword.put(:colors, enabled: enabled?)

      :error ->
        opts
    end
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
    if File.dir?("test"), do: ["test"], else: []
  end

  ## Shuffling

  defp shuffle(0, _algo, list), do: list

  defp shuffle(seed, algo, list) do
    :rand.seed(algo, {seed, seed, seed})
    Enum.shuffle(list)
  end
end
