defmodule Toast.Utils do
  @moduledoc "Compact utilities for filtering nil values from lists and joining."

  @doc false
  @spec compact([term()]) :: [term()]
  def compact(list), do: Enum.reject(list, &is_nil/1)

  @doc false
  @spec compact_join([term()], String.t()) :: String.t()
  def compact_join(list, joiner \\ ""), do: list |> compact() |> Enum.join(joiner)
end
