defmodule Toast.Diagnostics do
  @moduledoc "Shared diagnostics utilities."

  @doc """
  Check if a diagnostics map has cluster structure (keyed by server ID).

  Cluster diagnostics are a map of `%{server_id => %{sanitizer_errors: ..., ...}}`.
  Single-server diagnostics are a flat map `%{sanitizer_errors: ..., ...}`.
  """
  @spec cluster_diagnostics?(map()) :: boolean()
  def cluster_diagnostics?(diagnostics) do
    case Map.keys(diagnostics) do
      [] ->
        false

      [first_key | _] ->
        is_binary(first_key) and is_map(Map.get(diagnostics, first_key)) and
          Map.has_key?(Map.get(diagnostics, first_key), :sanitizer_errors)
    end
  end
end
