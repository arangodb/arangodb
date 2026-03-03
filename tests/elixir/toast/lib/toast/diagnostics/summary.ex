defmodule Toast.Diagnostics.Summary do
  @moduledoc "Format diagnostics into human-readable CLI summary sections."

  alias Toast.Deployment.ServerInstance
  alias ToastTest.ResultExporter.Shared

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
    diagnostics
    |> Toast.Diagnostics.to_server_entries()
    |> Enum.filter(fn {_id, diag} -> has_crash?(diag) end)
    |> Enum.map(fn {_id, diag} -> {diag, diag.server} end)
  end

  defp has_crash?(diag) do
    crash = Map.get(diag, :log_report)
    error = Map.get(diag, :server_error)
    (crash != nil and crash.signal_name != nil) or crash_error?(error)
  end

  defp crash_error?({:server_crashed, _}), do: true
  defp crash_error?({:server_unhealthy, _}), do: true
  defp crash_error?(_), do: false

  defp format_server_crash({diag, server}) do
    crash = Map.get(diag, :log_report)
    error = Map.get(diag, :server_error)

    [
      [format_server_header(server)],
      format_signal(crash, error),
      format_crash_output(crash),
      format_fatal_lines(crash),
      format_log_path(diag.server.log_file)
    ]
    |> Enum.concat()
    |> Enum.join("\n")
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

  # --- Attribution formatting (shared by crash and sanitizer) ---

  @max_content_lines 10

  @doc """
  Format crash report attribution for CLI display.

  Returns a formatted string with a "CRASH ATTRIBUTION" section, or nil if
  no crash attribution data is present. Shows matched crashes grouped by test
  case with confidence level, and unmatched crashes separately.
  """
  @spec format_crash_attribution(map(), [map()]) :: String.t() | nil
  def format_crash_attribution(crash_matching, crash_affected_tests \\ [])

  def format_crash_attribution(%{matched: [], unmatched: []}, []), do: nil

  def format_crash_attribution(%{matched: matched, unmatched: unmatched}, crash_affected_tests) do
    sections =
      format_matched_sections(matched, :crash, &format_crash_entry/1) ++
        format_unmatched_section(unmatched, &format_crash_entry/1) ++
        format_crash_affected(crash_affected_tests)

    wrap_attribution_banner("CRASH ATTRIBUTION", IO.ANSI.red(), sections)
  end

  def format_crash_attribution(_, _), do: nil

  @doc """
  Format sanitizer error attribution for CLI display.

  Returns a formatted string with a "SANITIZER ISSUES" section, or nil if
  no sanitizer issues were detected. Shows matched errors grouped by test case
  with confidence level, and unmatched errors separately.
  """
  @spec format_sanitizer_issues(map()) :: String.t() | nil
  def format_sanitizer_issues(%{matched: [], unmatched: []}), do: nil

  def format_sanitizer_issues(%{matched: matched, unmatched: unmatched}) do
    sections =
      format_matched_sections(matched, :error, &format_sanitizer_entry/1) ++
        format_unmatched_section(unmatched, &format_sanitizer_entry/1)

    wrap_attribution_banner("SANITIZER ISSUES", IO.ANSI.yellow(), sections)
  end

  def format_sanitizer_issues(_), do: nil

  defp wrap_attribution_banner(_title, _color, []), do: nil

  defp wrap_attribution_banner(title, color, sections) do
    IO.iodata_to_binary([
      "\n",
      color,
      @separator,
      "\n #{title}\n",
      @separator,
      IO.ANSI.reset(),
      "\n\n",
      Enum.join(sections, "\n\n"),
      "\n"
    ])
  end

  defp format_matched_sections([], _item_key, _format_entry_fn), do: []

  defp format_matched_sections(matched, item_key, format_entry_fn) do
    matched
    |> Shared.format_grouped_matches(item_key, format_entry_fn)
    |> Enum.map(fn {header, details} ->
      Enum.join(["  #{header}:" | details], "\n")
    end)
  end

  defp format_unmatched_section([], _format_entry_fn), do: []

  defp format_unmatched_section(unmatched, format_entry_fn) do
    header = "  Not attributed to a specific test:"
    details = Enum.map(unmatched, format_entry_fn)
    [Enum.join([header | details], "\n")]
  end

  defp format_crash_entry(crash) do
    signal = "#{crash.signal_name} (signal #{crash.signal_number})"
    preview = truncate_content(Enum.join(crash.crash_output, "\n"))

    [
      "    [#{signal}] #{crash.server_id}",
      indent(preview, 6),
      if(crash.log_file, do: "    (see #{crash.log_file})")
    ]
    |> Enum.reject(&is_nil/1)
    |> Enum.join("\n")
  end

  defp format_sanitizer_entry(error) do
    type = error.sanitizer_type |> Atom.to_string() |> String.upcase()
    preview = truncate_content(error.content)
    file_ref = "    (see #{error.file_path})"

    Enum.join(["    [#{type}] #{error.server_id}", indent(preview, 6), file_ref], "\n")
  end

  defp format_crash_affected([]), do: []

  defp format_crash_affected(tests) do
    entries =
      tests
      |> Enum.sort_by(fn t -> {inspect(t.module), t.name} end)
      |> Enum.map_join("\n", fn t -> "    #{inspect(t.module)} - #{t.name}" end)

    ["  Test failures caused by server crash (not actual test issues):\n#{entries}"]
  end

  defp truncate_content(content) do
    lines = String.split(content, "\n")

    if length(lines) > @max_content_lines do
      taken = Enum.take(lines, @max_content_lines)
      remaining = length(lines) - @max_content_lines
      Enum.join(taken, "\n") <> "\n... (#{remaining} more lines)"
    else
      String.trim_trailing(content)
    end
  end

  defp indent(text, n) do
    pad = String.duplicate(" ", n)

    text
    |> String.split("\n")
    |> Enum.map_join("\n", &(pad <> &1))
  end
end
