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

defmodule Toast.Utils do
  @moduledoc "Compact utilities for filtering nil values from lists and joining."

  @doc false
  @spec compact([term()]) :: [term()]
  def compact(list), do: Enum.reject(list, &is_nil/1)

  @doc false
  @spec compact_join([term()], String.t()) :: String.t()
  def compact_join(list, joiner \\ ""), do: list |> compact() |> Enum.join(joiner)

  @doc "Flatten a list of `{key, value}` tuples into a flat list."
  @spec flatten_opts([{String.t(), String.t()}]) :: [String.t()]
  def flatten_opts(opts), do: Enum.flat_map(opts, &Tuple.to_list/1)

  @doc "Put `key`/`value` into `map` only when `value` is non-nil."
  @spec maybe_put(map(), term(), term()) :: map()
  def maybe_put(map, _key, nil), do: map
  def maybe_put(map, key, value), do: Map.put(map, key, value)

  @doc "Milliseconds remaining until a monotonic deadline, minimum 0."
  @spec remaining_ms(integer()) :: non_neg_integer()
  def remaining_ms(deadline) do
    max(0, deadline - System.monotonic_time(:millisecond))
  end

  @doc "Naive English pluralization: appends \"s\" when `count != 1`."
  @spec pluralize(integer(), String.t()) :: String.t()
  def pluralize(1, word), do: word
  def pluralize(_count, word), do: word <> "s"
end
