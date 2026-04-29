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

defmodule ToastTest.JavascriptSuite do
  @moduledoc """
  Suite type for running legacy JavaScript tests via arangosh.

  Builds on `ToastTest.Suite` for deployment management and adds JS-specific
  configuration. The mix task detects JS suites and generates one Elixir module
  per JS test file for integration with Toast's pipeline.

  ## Usage

      defmodule ShellClientAql.Suite do
        use ToastTest.JavascriptSuite,
          paths: ["tests/js/client/aql", "tests/js/common/aql"],
          mode: :auto,
          server_args: %{"log.level" => "debug"},
          js_extra_args: %{"agency.supervision-ok-threshold" => "15"}
      end

  ## Options

  Accepts all `ToastTest.Suite` options plus:

    * `:paths` (required) — list of directories containing JS test files,
      relative to the repository root
    * `:js_extra_args` — map of extra arangosh arguments passed to the JS
      test runner (default: `%{}`)
    * `:weights` — map of `%{"filename.js" => weight}` for bucketing
      (default: `%{}`, unspecified files get weight 1)

  """

  defmacro __using__(opts) do
    {js_paths, opts} = Keyword.pop!(opts, :paths)
    {js_extra_args, opts} = Keyword.pop(opts, :js_extra_args, Macro.escape(%{}))
    {js_weights, suite_opts} = Keyword.pop(opts, :weights, Macro.escape(%{}))

    js_extra_args = ensure_escaped(js_extra_args)
    js_weights = ensure_escaped(js_weights)

    quote do
      use ToastTest.Suite, unquote(suite_opts)

      def __toast_js_paths__, do: unquote(js_paths)
      def __toast_js_extra_args__, do: unquote(js_extra_args)
      def __toast_js_weights__, do: unquote(js_weights)
    end
  end

  # Keyword.pop from macro opts returns AST (a tuple) when the caller passes
  # a variable, and a literal value otherwise. Escape literals so they can be
  # safely spliced into the quote block.
  defp ensure_escaped(value) when is_tuple(value), do: value
  defp ensure_escaped(value), do: Macro.escape(value)
end
