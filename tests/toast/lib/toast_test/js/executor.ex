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

defmodule ToastTest.JS.Executor do
  @moduledoc """
  Behaviour for executing JavaScript test files via arangosh.

  Implementations receive a JS test file path and options describing the
  deployment (endpoint, auth, paths) and return structured test results.
  """

  @type test_result :: %{
          name: String.t(),
          status: :pass | :fail | :skip | :error,
          duration_ms: non_neg_integer(),
          message: String.t() | nil,
          file: String.t() | nil,
          line: non_neg_integer() | nil
        }

  @type result :: %{tests: [test_result()]}

  @doc """
  Execute a JavaScript test file and return parsed results.

  ## Options

    * `:endpoint` - ArangoDB endpoint URL
    * `:auth` - Authentication config
    * `:arangosh_path` - Path to arangosh binary
    * `:working_dir` - Working directory for arangosh process
    * `:result_file` - Path where arangosh writes the JSON result
    * `:timeout` - Execution timeout in milliseconds
    * `:extra_args` - Additional arangosh command-line arguments

  """
  @callback run(js_file :: String.t(), opts :: keyword()) ::
              {:ok, result()} | {:error, term()}
end
