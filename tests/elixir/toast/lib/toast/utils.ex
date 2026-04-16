defmodule Toast.Utils do
  @moduledoc "Compact utilities for filtering nil values from lists and joining."

  @doc false
  @spec compact([term()]) :: [term()]
  def compact(list), do: Enum.reject(list, &is_nil/1)

  @doc false
  @spec compact_join([term()], String.t()) :: String.t()
  def compact_join(list, joiner \\ ""), do: list |> compact() |> Enum.join(joiner)

  @doc "Put `key`/`value` into `map` only when `value` is non-nil."
  @spec maybe_put(map(), term(), term()) :: map()
  def maybe_put(map, _key, nil), do: map
  def maybe_put(map, key, value), do: Map.put(map, key, value)

  @doc "Naive English pluralization: appends \"s\" when `count != 1`."
  @spec pluralize(integer(), String.t()) :: String.t()
  def pluralize(1, word), do: word
  def pluralize(_count, word), do: word <> "s"
end
