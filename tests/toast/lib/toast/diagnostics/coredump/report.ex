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

defmodule Toast.Diagnostics.Coredump.Report do
  @moduledoc "Structured report from coredump analysis."

  @type t :: %__MODULE__{
          core_path: Path.t(),
          binary_path: Path.t(),
          debugger: :gdb | :lldb,
          signal: String.t() | nil,
          faulting_address: String.t() | nil,
          registers: String.t() | nil,
          disassembly: String.t() | nil,
          threads: [map()],
          crash_thread: integer() | nil
        }

  defstruct [
    :core_path,
    :binary_path,
    :debugger,
    :signal,
    :faulting_address,
    :registers,
    :disassembly,
    :crash_thread,
    threads: []
  ]
end
