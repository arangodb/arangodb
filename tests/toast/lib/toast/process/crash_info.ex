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

defmodule Toast.Process.CrashInfo do
  @moduledoc """
  Facts about a process death, stamped by the `ServerProcess` that owned it.

  `executable` is the binary the dead incarnation was actually spawned with —
  recorded at crash time from the spawning process's own state, so it stays
  correct when restarts change the binary (upgrade scenarios).
  """

  @enforce_keys [:exit_status, :signal, :timestamp]
  defstruct [:exit_status, :signal, :timestamp, :os_pid, :executable]

  @type t :: %__MODULE__{
          exit_status: non_neg_integer() | nil,
          signal: non_neg_integer() | nil,
          timestamp: Toast.timestamp(),
          os_pid: pos_integer() | nil,
          executable: Path.t() | nil
        }
end
