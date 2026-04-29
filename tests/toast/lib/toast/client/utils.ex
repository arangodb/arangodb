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

defmodule Toast.Client.Utils do
  @moduledoc false

  @doc """
  Translates a keyword list of opts using a key map, filtering out unknown keys.

  Keys present in `key_map` are translated to their mapped values;
  keys not in `key_map` are silently dropped.
  """
  @spec translate_opts(keyword(), %{atom() => atom() | String.t()}) :: [
          {atom() | String.t(), term()}
        ]
  def translate_opts(opts, key_map) do
    for {k, v} <- opts, mapped = Map.get(key_map, k), do: {mapped, v}
  end
end
