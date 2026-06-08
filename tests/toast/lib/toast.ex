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

defmodule Toast do
  @moduledoc """
  Integration testing framework for ArangoDB.

  Toast manages ArangoDB server deployments (single server and cluster),
  runs tests against them, and provides diagnostics when things go wrong.
  """

  @typedoc "Unix timestamp in microseconds."
  @type timestamp :: integer()

  @doc "Return the current wall-clock time as a `t:timestamp/0`."
  @spec get_timestamp() :: timestamp()
  def get_timestamp, do: :os.system_time(:microsecond)
end
