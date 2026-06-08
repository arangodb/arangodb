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

defmodule ToastTest.Config do
  @moduledoc """
  Configuration for test execution — timeouts, result directories,
  diagnostics, and CI settings.

  This is intentionally separate from `Toast.Deployment.Config`, which holds
  deployment infrastructure concerns. See that module's docs for the rationale
  behind the split.

  Constructed via `new/1` which reads defaults from application env
  (populated by `Toast.Env.load/1`) and applies optional overrides.
  """

  @type t :: %__MODULE__{
          base_dir: Path.t(),
          result_dir: Path.t(),
          deployment_mode: :single_server | :cluster,
          timeout_factor: number(),
          global_timeout: pos_integer(),
          test_timeout: pos_integer(),
          keep_data: boolean(),
          ci: boolean(),
          force_all_tiers: boolean(),
          debugger: :gdb | :lldb | :auto | :none | nil,
          attach_debugger: boolean(),
          coredump_timeout: pos_integer(),
          coredump_dir: Path.t() | nil,
          dump_agency_on_error: boolean(),
          active_sanitizers: MapSet.t(String.t())
        }

  defstruct base_dir: nil,
            result_dir: Toast.Env.default_result_dir(),
            deployment_mode: :single_server,
            timeout_factor: 1,
            global_timeout: 3_600_000,
            test_timeout: 300_000,
            keep_data: false,
            ci: false,
            force_all_tiers: false,
            debugger: :auto,
            attach_debugger: false,
            coredump_timeout: 180_000,
            coredump_dir: nil,
            dump_agency_on_error: true,
            active_sanitizers: MapSet.new()

  @doc """
  Build a test config from application env with optional overrides.
  """
  @spec new(keyword()) :: t()
  def new(overrides \\ []) do
    struct!(Toast.Env.struct_from_env(%__MODULE__{}), overrides)
  end
end
