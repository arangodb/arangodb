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
