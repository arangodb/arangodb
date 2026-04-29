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

defmodule ToastTest.JS.ModuleBuilder do
  @moduledoc """
  Generates Elixir modules from JavaScript test file paths.

  Each JS file becomes a module with `__ex_unit__/0`, `__toast_suite__/0`,
  `__toast_js_file__/0`, and `__toast_weight__/0`. These modules participate
  in Toast's existing pipeline (filtering, bucketing, event emission) without
  using ExUnit.Case.
  """

  @segment_tags [
    {"cluster", :cluster_only},
    {"noncluster", :single_only},
    {"nightly", :nightly},
    {"timecritical", :timecritical},
    {"geo", :geo},
    {"nondeterministic", :nondeterministic},
    {"grey", :grey},
    {"graph", :graph},
    {"memoryintense", :memoryintense},
    {"novalgrind", :novalgrind},
    {"noarm", :noarm},
    {"noinstr", :noinstr},
    {"fp", :failure_points},
    {"noasan", :noasan},
    {"nocov", :nocov},
    {"sjs", :server_js}
  ]

  @spec build_modules(module(), [String.t()], keyword()) :: [module()]
  def build_modules(suite_module, js_files, opts \\ []) do
    Enum.map(js_files, &build_module(suite_module, &1, opts))
  end

  @spec build_module(module(), String.t(), keyword()) :: module()
  def build_module(suite_module, js_file, opts \\ []) do
    filename = Path.basename(js_file)
    module_name = derive_module_name(suite_module, filename)
    weight = Keyword.get(opts, :weight, 1)
    suffix_tags = tags_from_filename(filename)

    test_tags =
      Map.merge(%{file: js_file, line: 0, test_type: :test}, suffix_tags)

    placeholder_test = %ExUnit.Test{
      name: :"test js_execution",
      module: module_name,
      state: nil,
      time: 0,
      tags: test_tags
    }

    module_tags =
      Map.merge(%{file: js_file, async: false, js_test: true}, suffix_tags)

    test_module_struct = %ExUnit.TestModule{
      file: js_file,
      name: module_name,
      setup_all?: false,
      state: nil,
      tags: module_tags,
      tests: [placeholder_test]
    }

    contents =
      quote do
        def __ex_unit__, do: unquote(Macro.escape(test_module_struct))
        def __toast_suite__, do: unquote(suite_module)
        def __toast_js_file__, do: unquote(js_file)
        def __toast_weight__, do: unquote(weight)
      end

    {:module, module_name, _bytecode, _} =
      Module.create(module_name, contents, Macro.Env.location(__ENV__))

    module_name
  end

  @doc """
  Extracts ExUnit tags from JS filename suffixes.

  Maps the old framework's filename-based filtering convention to ExUnit tags.
  Segments are separated by hyphens, e.g. `aql-join-cluster.js` contains
  the segment `cluster` and gets `%{cluster_only: true}`.
  """
  @spec tags_from_filename(String.t()) :: map()
  def tags_from_filename(filename) do
    segments =
      filename
      |> String.trim_trailing(".js")
      |> String.split("-")
      |> MapSet.new()

    Enum.reduce(@segment_tags, %{}, fn {segment, tag_key}, acc ->
      if MapSet.member?(segments, segment) do
        Map.put(acc, tag_key, true)
      else
        acc
      end
    end)
  end

  @doc "Derives an Elixir module name from a suite module and JS filename."
  @spec derive_module_name(module(), String.t()) :: module()
  def derive_module_name(suite_module, filename) do
    base =
      filename
      |> String.trim_leading("test_")
      |> String.trim_trailing(".js")
      |> String.split(~r/[_\-]+/)
      |> Enum.map_join(&String.capitalize/1)

    suite_parts = Module.split(suite_module) |> Enum.drop(-1)
    Module.concat(suite_parts ++ ["JS", "#{base}Test"])
  end
end
