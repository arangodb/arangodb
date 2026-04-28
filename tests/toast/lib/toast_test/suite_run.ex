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

defmodule ToastTest.SuiteRun do
  @moduledoc false

  defstruct [
    :suite_module,
    :deployment_mode,
    :suite_deadline,
    :suite_timeout,
    :global_deadline,
    :global_timeout,
    :timeout_factor,
    :test_config,
    between_tests: :default
  ]

  @type t :: %__MODULE__{
          suite_module: module(),
          deployment_mode: :cluster | :single_server | :manual,
          suite_deadline: integer() | nil,
          suite_timeout: pos_integer() | nil,
          global_deadline: integer() | nil,
          global_timeout: pos_integer() | nil,
          timeout_factor: number(),
          test_config: ToastTest.Config.t(),
          between_tests: :default | false
        }
end
