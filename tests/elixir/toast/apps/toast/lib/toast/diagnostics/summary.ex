defmodule Toast.Diagnostics.Summary do
  @moduledoc "Format diagnostics into human-readable CLI summary sections."

  alias Toast.Deployment.ServerInstance

  @separator String.duplicate("=", 80)

  @doc """
  Format crashed server information for CLI display.

  Returns a formatted string with a "CRASHED SERVERS" section, or nil if
  no server crashes were detected. Handles both single-server and cluster
  diagnostics structures.
  """
  @spec format_crashed_servers(map() | nil) :: String.t() | nil
  def format_crashed_servers(nil), do: nil

  def format_crashed_servers(diagnostics) do
    entries = crashed_server_entries(diagnostics)

    if entries == [] do
      nil
    else
      sections =
        entries
        |> Enum.sort_by(fn {_diag, server} -> server.id end)
        |> Enum.map_join("\n\n", &format_server_crash/1)

      IO.iodata_to_binary([
        "\n",
        IO.ANSI.red(),
        @separator,
        "\n CRASHED SERVERS\n",
        @separator,
        IO.ANSI.reset(),
        "\n\n",
        sections,
        "\n"
      ])
    end
  end

  defp crashed_server_entries(diagnostics) do
    if Toast.ResultExporter.cluster_diagnostics?(diagnostics) do
      diagnostics
      |> Enum.filter(fn {_id, diag} -> has_crash?(diag) end)
      |> Enum.map(fn {_id, diag} -> {diag, diag.server} end)
    else
      if has_crash?(diagnostics) do
        [{diagnostics, diagnostics.server}]
      else
        []
      end
    end
  end

  defp has_crash?(diag) do
    crash = Map.get(diag, :crash_report)
    error = Map.get(diag, :server_error)
    (crash != nil and crash.signal_name != nil) or crash_error?(error)
  end

  defp crash_error?({:server_crashed, _}), do: true
  defp crash_error?({:server_unhealthy, _}), do: true
  defp crash_error?(_), do: false

  defp format_server_crash({diag, server}) do
    crash = Map.get(diag, :crash_report)
    error = Map.get(diag, :server_error)

    parts = [format_server_header(server)]
    parts = parts ++ format_signal(crash, error)
    parts = parts ++ format_crash_output(crash)
    parts = parts ++ format_fatal_lines(crash)
    parts = parts ++ format_log_path(diag.server.log_file)

    Enum.join(parts, "\n")
  end

  defp format_server_header(%ServerInstance{id: id, pid: pid, endpoint: endpoint}) do
    details =
      [if(pid, do: "PID #{pid}"), endpoint]
      |> Enum.reject(&is_nil/1)

    case details do
      [] -> "  #{id}:"
      parts -> "  #{id} (#{Enum.join(parts, ", ")}):"
    end
  end

  defp format_signal(%{signal_name: name, signal_number: num}, _) when not is_nil(name) do
    ["    Signal: #{name} (signal #{num})"]
  end

  defp format_signal(_, {:server_crashed, %{signal: sig, exit_status: es}})
       when not is_nil(sig) do
    ["    Exit: signal #{sig}, exit_status #{es}"]
  end

  defp format_signal(_, {:server_crashed, %{exit_status: es}}) when not is_nil(es) do
    ["    Exit: exit_status #{es}"]
  end

  defp format_signal(_, {:server_unhealthy, _}) do
    ["    Server became unresponsive"]
  end

  defp format_signal(_, _), do: ["    Crashed (no details available)"]

  defp format_crash_output(%{crash_output: lines}) when is_list(lines) and lines != [] do
    formatted = Enum.map(lines, &"      #{&1}")
    ["    Crash output:" | formatted]
  end

  defp format_crash_output(_), do: ["    No crash information found in server log."]

  defp format_fatal_lines(%{fatal_lines: lines}) when is_list(lines) and lines != [] do
    formatted = Enum.map(lines, &"      #{&1}")
    ["    Fatal log entries:" | formatted]
  end

  defp format_fatal_lines(_), do: []

  defp format_log_path(path) when is_binary(path), do: ["    Log: #{path}"]
  defp format_log_path(_), do: []
end
