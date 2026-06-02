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

defmodule ToastTest.Interactive.Resolver do
  @moduledoc false

  @spec resolve(module() | String.t()) :: {module(), [module()]}
  def resolve(path) when is_binary(path) do
    path = Path.expand(path)
    suite_root = find_suite_root!(path)

    if File.dir?(path) do
      {suite_module, all_test_modules} = compile_all(suite_root)

      test_modules =
        if suite_root == path,
          do: all_test_modules,
          else: filter_modules_under(all_test_modules, path)

      {suite_module, test_modules}
    else
      compile_for_file(suite_root, path)
    end
  end

  def resolve(module) when is_atom(module) do
    case try_resolve_suite_dir(module) do
      nil -> resolve_loaded(module)
      suite_dir -> resolve_from_suite_dir(module, suite_dir)
    end
  end

  @spec find_suite_root!(String.t()) :: String.t()
  def find_suite_root!(path) do
    dir = if File.dir?(path), do: path, else: Path.dirname(path)
    do_find_suite_root(dir, path)
  end

  defp do_find_suite_root(dir, original) do
    if File.exists?(Path.join(dir, "suite.ex")) do
      dir
    else
      parent = Path.dirname(dir)

      if parent == dir do
        raise ArgumentError, "no suite.ex found in #{original} or any parent directory"
      end

      do_find_suite_root(parent, original)
    end
  end

  @spec compile_all(String.t()) :: {module(), [module()]}
  def compile_all(suite_dir) do
    suite_module = compile_suite(suite_dir)
    test_modules = compile_test_files(suite_dir)

    if test_modules == [] do
      raise ArgumentError, "no test modules found in #{suite_dir}"
    end

    {suite_module, test_modules}
  end

  @spec compile_suite(String.t()) :: module()
  def compile_suite(suite_dir) do
    suite_file = Path.join(suite_dir, "suite.ex")

    unless File.exists?(suite_file) do
      raise ArgumentError, "no suite.ex found in #{suite_dir}"
    end

    suite_module = suite_file |> recompile_file() |> hd()

    suite_dir
    |> Path.join("**/*.ex")
    |> Path.wildcard()
    |> Enum.reject(&(&1 == suite_file))
    |> Enum.each(&recompile_file/1)

    suite_module
  end

  @spec compile_test_files(String.t(), keyword()) :: [module()]
  def compile_test_files(suite_dir, opts \\ []) do
    exclude = opts[:exclude]

    suite_dir
    |> Path.join("**/test_*.exs")
    |> Path.wildcard()
    |> Enum.reject(&(&1 == exclude))
    |> Enum.sort()
    |> Enum.flat_map(&recompile_file/1)
  end

  @spec filter_modules_under([module()], String.t()) :: [module()]
  def filter_modules_under(modules, dir) do
    dir_prefix = dir <> "/"

    Enum.filter(modules, fn mod ->
      case source_path(mod) do
        nil -> false
        source -> String.starts_with?(source, dir_prefix) or Path.dirname(source) == dir
      end
    end)
  end

  defp compile_for_file(suite_root, target_path) do
    suite_module = compile_suite(suite_root)
    compile_test_files(suite_root, exclude: target_path)
    module = target_path |> recompile_file() |> hd()
    {suite_module, [module]}
  end

  defp recompile_file(path) do
    prev = Code.compiler_options()[:ignore_module_conflict]
    Code.compiler_options(ignore_module_conflict: true)

    try do
      path |> Code.compile_file() |> Enum.map(&elem(&1, 0))
    after
      Code.compiler_options(ignore_module_conflict: prev)
    end
  end

  defp resolve_from_suite_dir(module, suite_dir) do
    {suite_module, test_modules} = compile_all(suite_dir)
    test_modules = if module == suite_module, do: test_modules, else: [module]
    {suite_module, test_modules}
  end

  defp resolve_loaded(module) do
    cond do
      suite_module?(module) ->
        test_modules = discover_loaded_test_modules(module)

        if test_modules == [] do
          raise ArgumentError, "no test modules found for suite #{inspect(module)}"
        end

        {module, test_modules}

      Code.ensure_loaded?(module) and test_module_with_suite?(module) ->
        {module.__toast_suite__(), [module]}

      true ->
        raise ArgumentError,
              "module #{inspect(module)} is not loaded or does not belong to a suite"
    end
  end

  defp suite_module?(module) do
    Code.ensure_loaded?(module) and function_exported?(module, :deployment_config, 0)
  end

  defp test_module_with_suite?(module) do
    function_exported?(module, :__toast_suite__, 0)
  end

  defp discover_loaded_test_modules(suite_module) do
    for {mod, _file} <- :code.all_loaded(),
        function_exported?(mod, :__toast_suite__, 0),
        mod.__toast_suite__() == suite_module do
      mod
    end
    |> Enum.sort()
  end

  defp try_resolve_suite_dir(module) do
    if Code.ensure_loaded?(module) do
      case source_path(module) do
        nil ->
          infer_suite_dir(module)

        source ->
          source_dir = Path.dirname(source)

          if File.exists?(Path.join(source_dir, "suite.ex")),
            do: source_dir,
            else: infer_suite_dir(module)
      end
    else
      infer_suite_dir(module)
    end
  end

  defp source_path(module) do
    case module.__info__(:compile)[:source] do
      nil -> nil
      source -> to_string(source)
    end
  end

  defp infer_suite_dir(module) do
    suite_name =
      module
      |> Module.split()
      |> hd()
      |> Macro.underscore()

    dir = Path.expand(Path.join("suites", suite_name))
    if File.dir?(dir), do: dir
  end
end
