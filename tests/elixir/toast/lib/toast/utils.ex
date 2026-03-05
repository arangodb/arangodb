defmodule Toast.Utils do
  @moduledoc "Small conditional-insertion helpers for building maps and lists."

  @doc """
  If the given value is not nil, inserts it under the specified key and returns
  the new map. Otherwise, returns the map unchanged.
  """
  def conditional_put(%{} = map, _key, nil), do: map
  def conditional_put(%{} = map, key, val), do: Map.put(map, key, val)

  def conditional_put(%{} = map, _key, _val, false), do: map
  def conditional_put(%{} = map, key, val, true), do: Map.put(map, key, val)

  def conditional_put(%{} = map, _key, nil, modifier) when is_function(modifier, 1), do: map

  def conditional_put(%{} = map, key, val, modifier) when is_function(modifier, 1),
    do: Map.put(map, key, modifier.(val))

  @doc false
  @spec compact([term()]) :: [term()]
  def compact(list), do: Enum.reject(list, &is_nil/1)

  @doc false
  @spec compact_join([term()], String.t()) :: String.t()
  def compact_join(list, joiner \\ ""), do: list |> compact() |> Enum.join(joiner)
end
