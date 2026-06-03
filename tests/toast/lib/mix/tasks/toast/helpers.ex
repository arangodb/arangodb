################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule Mix.Tasks.Toast.Helpers do
  @moduledoc false

  # Pure helper functions extracted from Mix.Tasks.Toast for testability.

  @doc """
  Strips the `suites/` prefix from a single argument if present.
  """
  @spec normalize_arg(String.t()) :: String.t()
  def normalize_arg("suites/" <> rest), do: rest
  def normalize_arg(arg), do: arg

  @doc """
  Expands arguments, resolving globs against `suites_dir` and stripping `suites/` prefixes.

  Non-glob arguments are returned with just prefix normalization.
  Glob arguments are expanded against the filesystem; if no files match, the
  normalized argument is returned as-is (letting downstream code report the error).
  """
  @spec expand_args([String.t()], String.t()) :: [String.t()]
  def expand_args(args, suites_dir) do
    Enum.flat_map(args, fn arg ->
      arg |> normalize_arg() |> expand_single_arg(suites_dir)
    end)
  end

  defp expand_single_arg(normalized, suites_dir) do
    if glob?(normalized) do
      case Path.join(suites_dir, normalized) |> Path.wildcard() do
        [] -> [normalized]
        paths -> Enum.map(paths, &Path.relative_to(&1, suites_dir))
      end
    else
      [normalized]
    end
  end

  defp glob?(arg), do: String.contains?(arg, ["*", "?", "["])

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
      :max_failures,
      :exit_status,
      :only_test_ids
    ]

    ex_unit_opts =
      opts
      |> filter_opts(:include)
      |> filter_opts(:exclude)
      |> filter_only()
      |> Keyword.put_new(:exit_status, 2)
      |> Keyword.take(option_keys)

    [autorun: false] ++ ex_unit_opts
  end

  @doc """
  Maps CLI option keys to `Toast.Env.load/1` keyword list keys.
  """
  @spec opts_to_env_list(keyword()) :: keyword()
  def opts_to_env_list(opts) do
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
      memory_budget: :memory_budget,
      keep_data: :keep_data,
      attach_debugger: :attach_debugger,
      sanitizer: :sanitizer_override,
      rr: :rr,
      cluster_agents: :cluster_agents,
      cluster_dbservers: :cluster_dbservers,
      cluster_coordinators: :cluster_coordinators,
      replication_factor: :cluster_replication_factor,
      ci: :ci,
      force_all_tiers: :force_all_tiers
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

    config_list =
      case Keyword.fetch(opts, :http2) do
        {:ok, true} -> Keyword.put(config_list, :protocol, :http2)
        {:ok, false} -> Keyword.put(config_list, :protocol, :http1)
        :error -> config_list
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
    all_tests = Enum.flat_map(test_modules, & &1.__ex_unit__().tests)

    for {filter_file, filter_line} <- line_filters,
        test = closest_test_at_or_before(all_tests, filter_file, filter_line),
        test != nil,
        into: MapSet.new() do
      {test.module, test.name}
    end
  end

  defp closest_test_at_or_before(tests, filter_file, filter_line) do
    tests
    |> Enum.filter(fn %{tags: tags} ->
      tags[:file] != nil and Path.basename(tags[:file]) == filter_file and
        tags[:line] <= filter_line
    end)
    |> Enum.max_by(fn %{tags: tags} -> tags[:line] end, fn -> nil end)
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
end
