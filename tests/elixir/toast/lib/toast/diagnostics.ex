defmodule Toast.Diagnostics do
  @moduledoc "Shared diagnostics utilities."

  @doc """
  Convert diagnostics to a uniform list of `{server_id, diag}` entries.

  Handles both the normalized format (`%{server_id => diag}`) and the
  legacy flat single-server format (`%{sanitizer_errors: ..., ...}`).
  Non-diagnostics top-level keys (e.g. `:agency_dump`, `:coredump_reports`)
  are filtered out.
  """
  @spec to_server_entries(map()) :: [{String.t(), map()}]
  def to_server_entries(diagnostics) when is_map(diagnostics) do
    if cluster_diagnostics?(diagnostics) do
      diagnostics
      |> Enum.filter(fn {key, val} -> is_binary(key) and is_map(val) end)
      |> Enum.to_list()
    else
      server_id =
        case diagnostics do
          %{server: %{id: id}} when is_binary(id) -> id
          _ -> "unknown"
        end

      [{server_id, diagnostics}]
    end
  end

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
