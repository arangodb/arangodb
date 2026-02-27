defmodule Mix.Tasks.Toast.Helpers do
  @moduledoc false

  # Pure helper functions extracted from Mix.Tasks.Toast for testability.

  @doc """
  Parses suite arguments into a suite request (`:all` or list of names)
  and a map of file filters keyed by suite name.
  """
  def parse_suite_args([], _suites_dir), do: {:all, %{}}

  def parse_suite_args(args, _suites_dir) do
    {suite_names, file_filters} =
      Enum.reduce(args, {[], %{}}, fn arg, {names, filters} ->
        case String.split(arg, "/", parts: 2) do
          [suite_name, file_spec] ->
            {[suite_name | names], Map.update(filters, suite_name, [file_spec], &[file_spec | &1])}

          [suite_name] ->
            {[suite_name | names], filters}
        end
      end)

    {Enum.uniq(Enum.reverse(suite_names)), file_filters}
  end

  @doc """
  Processes parsed CLI opts into ExUnit-compatible options.
  """
  def process_opts(opts) do
    option_keys = [
      :include,
      :exclude,
      :trace,
      :max_cases,
      :timeout,
      :max_failures,
      :formatters,
      :colors,
      :exit_status,
      :only_test_ids
    ]

    ex_unit_opts =
      opts
      |> filter_opts(:include)
      |> filter_opts(:exclude)
      |> filter_only()
      |> formatter_opts()
      |> color_opts()
      |> Keyword.put_new(:exit_status, 2)
      |> Keyword.take(option_keys)

    [autorun: false] ++ ex_unit_opts
  end

  @doc """
  Maps CLI option keys to Toast.Config keyword list keys.
  """
  def opts_to_config_list(opts) do
    mapping = [
      build_dir: :build_dir,
      work_dir: :work_dir,
      result_dir: :result_dir,
      show_server_logs: :show_server_logs,
      global_timeout: :global_timeout,
      test_timeout: :test_timeout,
      startup_timeout: :startup_timeout,
      shutdown_timeout: :shutdown_timeout,
      timeout_factor: :timeout_factor,
      keep_work_dir: :keep_work_dir,
      sanitizer: :explicit_sanitizer,
      cluster_agents: :cluster_agents,
      cluster_dbservers: :cluster_dbservers,
      cluster_coordinators: :cluster_coordinators,
      replication_factor: :cluster_replication_factor,
      ci: :ci
    ]

    config_list =
      for {cli_key, config_key} <- mapping,
          {:ok, value} <- [Keyword.fetch(opts, cli_key)] do
        {config_key, value}
      end

    cond do
      opts[:cluster] -> Keyword.put(config_list, :deployment_mode, :cluster)
      opts[:single] -> Keyword.put(config_list, :deployment_mode, :single_server)
      true -> config_list
    end
  end

  @doc """
  Classifies files in a suite directory into helpers (.ex, excluding suite.ex)
  and test files (.exs starting with test_).
  """
  def discover_suite_files(suite_dir) do
    all_files = Path.wildcard(Path.join(suite_dir, "*"))

    helpers =
      all_files
      |> Enum.filter(&(String.ends_with?(&1, ".ex") and Path.basename(&1) != "suite.ex"))

    test_files =
      all_files
      |> Enum.filter(&String.ends_with?(&1, ".exs"))
      |> Enum.filter(&(Path.basename(&1) |> String.starts_with?("test_")))

    {helpers, test_files}
  end

  @doc """
  Filters test files based on file filter specs from CLI arguments.
  Returns `{filtered_files, line_filters}`.
  """
  def apply_file_filters(test_files, filters, _suite_dir) when map_size(filters) == 0,
    do: {test_files, []}

  def apply_file_filters(test_files, filters, suite_dir) do
    suite_name = Path.basename(suite_dir)

    case Map.get(filters, suite_name) do
      nil ->
        {test_files, []}

      file_specs ->
        {file_names, line_filters} = parse_file_specs(file_specs)

        filtered =
          Enum.filter(test_files, fn path ->
            Path.basename(path) in file_names
          end)

        {filtered, line_filters}
    end
  end

  @doc """
  Checks whether any suite in the results has sanitizer errors.
  Handles both single-server and cluster diagnostics layouts.
  """
  def has_sanitizer_errors?(suites) do
    Enum.any?(suites, fn suite ->
      diag = suite[:diagnostics]
      diag != nil and has_sanitizer_in_diagnostics?(diag)
    end)
  end

  @doc """
  Builds suite diagnostics from suite results for CI packaging.
  """
  def build_suite_diagnostics(suites) do
    Enum.map(suites, fn suite ->
      %{
        name: suite[:suite_module] |> inspect(),
        log_files: extract_log_files(suite[:diagnostics]),
        sanitizer_files: extract_sanitizer_files(suite[:diagnostics]),
        crash_reports: [],
        agency_dumps: [],
        core_dumps: extract_core_dumps(suite[:diagnostics])
      }
    end)
  end

  @doc """
  Builds per-suite options from test modules, line filters, and test name pattern.
  """
  def build_suite_opts(test_modules, line_filters, test_name_pattern) do
    opts = []

    opts =
      if test_name_pattern != nil do
        Keyword.put(opts, :test_name_pattern, test_name_pattern)
      else
        opts
      end

    if line_filters != [] do
      only_ids = build_only_test_ids(test_modules, line_filters)
      Keyword.put(opts, :only_test_ids, only_ids)
    else
      opts
    end
  end

  @doc """
  Parses file specs like `["test_foo.exs", "test_bar.exs:42"]` into
  `{file_names, line_filters}`.
  """
  def parse_file_specs(file_specs) do
    Enum.reduce(file_specs, {[], []}, fn spec, {files, lines} ->
      case String.split(spec, ":") do
        [file, line_str] ->
          case Integer.parse(line_str) do
            {line, ""} -> {[file | files], [{file, line} | lines]}
            _ -> {[file | files], lines}
          end

        [file] ->
          {[file | files], lines}
      end
    end)
  end

  # -- Internal helpers --

  defp has_sanitizer_in_diagnostics?(diagnostics) when is_map(diagnostics) do
    case Map.get(diagnostics, :sanitizer_errors) do
      errors when is_list(errors) and errors != [] ->
        true

      _ ->
        Enum.any?(diagnostics, fn
          {_id, server_diag} when is_map(server_diag) ->
            case Map.get(server_diag, :sanitizer_errors) do
              errors when is_list(errors) and errors != [] -> true
              _ -> false
            end

          _ ->
            false
        end)
    end
  end

  defp has_sanitizer_in_diagnostics?(_), do: false

  defp extract_log_files(diagnostics) do
    extract_from_diagnostics(diagnostics, :server, fn
      %{log_file: path} when is_binary(path) -> [path]
      _ -> []
    end)
  end

  defp extract_sanitizer_files(diagnostics) do
    extract_from_diagnostics(diagnostics, :sanitizer_errors, fn
      errors when is_list(errors) ->
        errors |> Enum.map(& &1.file_path) |> Enum.filter(&is_binary/1)

      _ ->
        []
    end)
  end

  defp extract_core_dumps(diagnostics) do
    extract_from_diagnostics(diagnostics, :coredump_reports, fn
      reports when is_list(reports) ->
        reports |> Enum.map(& &1.core_path) |> Enum.filter(&is_binary/1)

      _ ->
        []
    end)
  end

  defp extract_from_diagnostics(nil, _key, _extractor), do: []

  defp extract_from_diagnostics(diagnostics, key, extractor) when is_map(diagnostics) do
    case Map.get(diagnostics, key) do
      nil ->
        diagnostics
        |> Enum.flat_map(fn
          {_id, server_diag} when is_map(server_diag) ->
            extractor.(Map.get(server_diag, key))

          _ ->
            []
        end)

      value ->
        extractor.(value)
    end
  end

  defp extract_from_diagnostics(_diagnostics, _key, _extractor), do: []

  defp build_only_test_ids(test_modules, line_filters) do
    for mod <- test_modules,
        test <- mod.__ex_unit__().tests,
        line_matches?(test, line_filters),
        into: MapSet.new() do
      {test.module, test.name}
    end
  end

  defp line_matches?(test, line_filters) do
    file_basename = test.tags[:file] && Path.basename(test.tags[:file])

    Enum.any?(line_filters, fn {filter_file, filter_line} ->
      file_basename == filter_file and test.tags[:line] == filter_line
    end)
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
end
