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

defmodule ToastTest.CrashEvent do
  @moduledoc """
  Structured crash event for post-execution analysis.

  Process-level facts (signal, exit status, pid, executable) live in
  `crash_info`, stamped by the `ServerProcess` that owned the dead process.
  `crash_info.timestamp` is the raw process-exit detection time and is never
  rewritten — it is the provenance.

  The remaining fields are filled by the enrichment phase
  (`ToastTest.Enrichment.enrich_crashes/3`) from the filesystem; they are nil
  / empty on a freshly-converted event:

    * `effective_at`   — the crash time resolved from the server log (the crash
                         handler's own timestamp), falling back to
                         `crash_info.timestamp` when no crash log line exists
                         (e.g. SIGKILL). This is the timestamp attribution and
                         log-window display use.
    * `crash_lines`    — the formatted trailing error/fatal log entries.
    * `log_file`       — the server log path the entries came from.
    * `coredump_reports` — analyzed coredump reports for this incarnation.
  """

  @enforce_keys [:server_id, :crash_info]
  defstruct [
    :server_id,
    :crash_info,
    :effective_at,
    :crash_lines,
    :log_file,
    expected: false,
    coredump_reports: []
  ]

  @type t :: %__MODULE__{
          server_id: String.t(),
          crash_info: Toast.Process.CrashInfo.t(),
          effective_at: Toast.timestamp() | nil,
          crash_lines: String.t() | nil,
          log_file: Path.t() | nil,
          expected: boolean(),
          coredump_reports: [map()]
        }
end
