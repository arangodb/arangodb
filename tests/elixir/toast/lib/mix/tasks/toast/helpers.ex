defmodule Mix.Tasks.Toast.Helpers do
  @moduledoc false

  # Pure helper functions extracted from Mix.Tasks.Toast for testability.

  @doc """
  Parses suite arguments into a suite request (`:all` or list of names)
  and a map of file filters keyed by suite name.
  """
  @spec parse_suite_args([String.t()]) ::
          {:all | [String.t()], %{optional(String.t()) => [String.t()]}}
  def parse_suite_args([]), do: {:all, %{}}

  def parse_suite_args(args) do
    {suite_names, file_filters} =
      Enum.reduce(args, {[], %{}}, fn arg, {names, filters} ->
        case String.split(arg, "/", parts: 2) do
          [suite_name, file_spec] ->
            {[suite_name | names],
             Map.update(filters, suite_name, [file_spec], &[file_spec | &1])}

          [suite_name] ->
            {[suite_name | names], filters}
        end
      end)

    {suite_names |> Enum.reverse() |> Enum.uniq(), file_filters}
  end

  @doc """
  Processes parsed CLI opts into ExUnit-compatible options.
  """
  @spec process_opts(keyword()) :: keyword()
  def process_opts(opts) do
    option_keys = [
      :include,
      :exclude,
      :trace,
      :timeout,
      :max_failures,
      :colors,
      :exit_status,
      :only_test_ids
    ]

    ex_unit_opts =
      opts
      |> filter_opts(:include)
      |> filter_opts(:exclude)
      |> filter_only()
      |> color_opts()
      |> Keyword.put_new(:exit_status, 2)
      |> Keyword.take(option_keys)

    [autorun: false] ++ ex_unit_opts
  end

  @doc """
  Maps CLI option keys to Toast.Config keyword list keys.
  """
  @spec opts_to_config_list(keyword()) :: keyword()
  def opts_to_config_list(opts) do
    mapping = [
      build_dir: :build_dir,
      base_dir: :base_dir,
      result_dir: :result_dir,
      show_server_logs: :show_server_logs,
      global_timeout: :global_timeout,
      test_timeout: :test_timeout,
      startup_timeout: :startup_timeout,
      shutdown_timeout: :shutdown_timeout,
      timeout_factor: :timeout_factor,
      keep_data: :keep_data,
      sanitizer: :sanitizer_override,
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

    config_list =
      if opts[:no_agency_dump],
        do: Keyword.put(config_list, :dump_agency_on_error, false),
        else: config_list

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
  @spec discover_suite_files(String.t()) :: {[String.t()], [String.t()]}
  def discover_suite_files(suite_dir) do
    all_files = suite_dir |> Path.join("*") |> Path.wildcard()

    helpers =
      Enum.filter(all_files, fn path ->
        String.ends_with?(path, ".ex") and Path.basename(path) != "suite.ex"
      end)

    test_files =
      Enum.filter(all_files, fn path ->
        String.ends_with?(path, ".exs") and
          path |> Path.basename() |> String.starts_with?("test_")
      end)

    {helpers, test_files}
  end

  @doc """
  Filters test files based on file filter specs from CLI arguments.
  Returns `{filtered_files, line_filters}`.
  """
  @spec apply_file_filters([String.t()], %{optional(String.t()) => [String.t()]}, String.t()) ::
          {[String.t()], [{String.t(), pos_integer()}]}
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
  Builds per-suite options from test modules, line filters, and test name pattern.
  """
  @spec build_suite_opts([module()], [{String.t(), pos_integer()}], String.t() | nil) :: keyword()
  def build_suite_opts(test_modules, line_filters, test_name_pattern) do
    [
      if(test_name_pattern != nil, do: {:test_name_pattern, test_name_pattern}),
      if(line_filters != [],
        do: {:only_test_ids, build_only_test_ids(test_modules, line_filters)}
      )
    ]
    |> Toast.Utils.compact()
  end

  @doc """
  Parses file specs like `["test_foo.exs", "test_bar.exs:42"]` into
  `{file_names, line_filters}`.
  """
  @spec parse_file_specs([String.t()]) :: {[String.t()], [{String.t(), pos_integer()}]}
  def parse_file_specs(file_specs) do
    Enum.reduce(file_specs, {[], []}, fn spec, {files, lines} ->
      parse_single_file_spec(spec, files, lines)
    end)
  end

  defp parse_single_file_spec(spec, files, lines) do
    case String.split(spec, ":") do
      [file, line_str] ->
        case Integer.parse(line_str) do
          {line, ""} -> {[file | files], [{file, line} | lines]}
          _ -> {[file | files], lines}
        end

      [file] ->
        {[file | files], lines}
    end
  end

  # -- Internal helpers --

  defp build_only_test_ids(test_modules, line_filters) do
    for mod <- test_modules,
        test <- mod.__ex_unit__().tests,
        line_matches?(test, line_filters),
        into: MapSet.new() do
      {test.module, test.name}
    end
  end

  defp line_matches?(%{tags: %{file: nil}}, _line_filters), do: false

  defp line_matches?(%{tags: tags}, line_filters) do
    file_basename = Path.basename(tags[:file])

    Enum.any?(line_filters, fn {filter_file, filter_line} ->
      file_basename == filter_file and tags[:line] == filter_line
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

  defp color_opts(opts) do
    case Keyword.fetch(opts, :color) do
      {:ok, enabled?} ->
        opts |> Keyword.delete(:color) |> Keyword.put(:colors, enabled: enabled?)

      :error ->
        opts
    end
  end
end
