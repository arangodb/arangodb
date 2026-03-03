defmodule Toast.Diagnostics do
  @moduledoc "Shared diagnostics utilities."

  alias Toast.Diagnostics.{LogAnalyzer, Sanitizer}

  @doc """
  Convert diagnostics to a uniform list of `{server_id, diag}` entries.

  Diagnostics are always `%{server_id => diag}`. This filters to entries
  with binary keys and map values (excluding non-diagnostics top-level
  keys like `:agency_dump`).
  """
  @spec to_server_entries(map()) :: [{String.t(), map()}]
  def to_server_entries(diagnostics) when is_map(diagnostics) do
    diagnostics
    |> Enum.filter(fn {key, val} -> is_binary(key) and is_map(val) end)
    |> Enum.to_list()
  end

  @doc """
  Build a diagnostics map for a single server.

  Collects sanitizer errors and parses the server log in a single pass.
  Returns `%{sanitizer_errors:, log_report:, server_error:, server:}`.
  """
  @spec build_server_diagnostics(struct(), term()) :: map()
  def build_server_diagnostics(server, server_error) do
    %{
      sanitizer_errors: Sanitizer.collect_errors(server.server_dir, server.id),
      log_report: LogAnalyzer.parse(server.log_file),
      server_error: server_error,
      server: server
    }
  end
end
