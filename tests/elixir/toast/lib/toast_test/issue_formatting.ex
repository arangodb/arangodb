defmodule ToastTest.IssueFormatting do
  @moduledoc false

  @max_sanitizer_lines 15
  @max_crash_log_lines 15
  @max_backtrace_frames 20

  # --- Coredump resolution ---

  @doc "Build `%{core_path => coredump_report}` lookup from a list of coredump reports."
  def build_coredump_index(coredumps) do
    Map.new(coredumps, &{&1.core_path, &1})
  end

  @doc "Resolve `:coredump_paths` in crash issues to full coredump reports."
  def resolve_coredumps(issues, coredump_index) do
    Enum.map(issues, &resolve_issue_coredumps(&1, coredump_index))
  end

  defp resolve_issue_coredumps(%{type: :crash, detail: detail} = issue, index) do
    paths = detail[:coredump_paths] || []
    coredumps = Enum.flat_map(paths, fn p -> if r = index[p], do: [r], else: [] end)
    %{issue | detail: Map.put(detail, :coredumps, coredumps)}
  end

  defp resolve_issue_coredumps(issue, _index), do: issue

  # --- Sanitizer ---

  def format_sanitizer(%{scope: scope, detail: detail}) do
    [
      format_attribution(scope, detail[:server]),
      truncate(detail[:report], @max_sanitizer_lines)
    ]
    |> Toast.Utils.compact_join("\n")
  end

  # --- Crash ---

  def format_crash(%{scope: scope, detail: detail}) do
    [
      format_attribution(scope, detail[:server]),
      format_crash_info(detail),
      format_crash_detail(detail),
      format_coredump_path(detail),
      format_log_path(detail)
    ]
    |> Toast.Utils.compact_join("\n")
  end

  def format_crash_info(%{server: server, crash_info: info}) do
    parts =
      [
        format_pid(Map.get(info, :os_pid)),
        format_signal(Map.get(info, :signal)),
        format_exit_status(Map.get(info, :exit_status)),
        format_timestamp(Map.get(info, :timestamp))
      ]
      |> Toast.Utils.compact()

    "#{server}: #{Enum.join(parts, "  ")}"
  end

  def format_crash_info(_), do: nil

  def format_crash_detail(%{crash_lines: crash_lines})
      when is_binary(crash_lines) do
    truncate(crash_lines, @max_crash_log_lines)
  end

  def format_crash_detail(%{coredumps: [coredump | _]}) do
    format_coredump_backtrace(coredump)
  end

  def format_crash_detail(_detail), do: nil

  # --- Timeout ---

  def format_timeout(%{detail: detail}) do
    header = "[#{timeout_source_label(detail[:source])}] #{detail[:reason]}"

    server_lines =
      for srv <- detail[:servers] || [] do
        pid_part = if srv[:os_pid], do: " (PID #{srv[:os_pid]})", else: ""

        [
          "  #{srv[:server_id]}#{pid_part}",
          if(srv[:log_file], do: "    Log: #{srv[:log_file]}"),
          if(srv[:coredump], do: "    Coredump: #{srv[:coredump]}")
        ]
      end

    [header | server_lines]
    |> List.flatten()
    |> Toast.Utils.compact_join("\n")
  end

  # --- Shared helpers ---

  def format_attribution(scope, server) do
    case format_scope(scope) do
      nil -> server
      label -> "#{server} \u2014 #{label}"
    end
  end

  def format_scope(:suite), do: nil
  def format_scope({:module, mod}), do: inspect(mod)

  def format_scope({:test, mod, name}) do
    "#{inspect(mod)} > \"#{ToastTest.Formatting.display_test_name(name)}\""
  end

  def timeout_source_label(:startup_timeout), do: "Startup Timeout"
  def timeout_source_label(:shutdown_timeout), do: "Shutdown Timeout"
  def timeout_source_label(:test_timeout), do: "Test Timeout"
  def timeout_source_label(:global_timeout), do: "Global Timeout"
  def timeout_source_label(other), do: "Timeout: #{other}"

  def truncate(nil, _max), do: nil

  def truncate(text, max_lines) do
    lines = String.split(text, "\n")
    shown = Enum.take(lines, max_lines)
    remaining = length(lines) - length(shown)
    suffix = if remaining > 0, do: ["... (#{remaining} more lines)"], else: []
    Enum.join(shown ++ suffix, "\n")
  end

  def format_pid(nil), do: nil
  def format_pid(pid), do: "PID #{pid}"

  def format_signal(nil), do: nil

  def format_signal(sig) do
    case :exec.signal(sig) do
      name when is_atom(name) ->
        "signal: #{name |> Atom.to_string() |> String.upcase()} (#{sig})"

      _ ->
        "signal: #{sig}"
    end
  end

  def format_exit_status(nil), do: nil
  def format_exit_status(status), do: "exit_status: #{status}"

  def format_timestamp(%DateTime{} = ts), do: "at: #{DateTime.to_iso8601(ts)}"

  def format_timestamp(us) when is_integer(us),
    do: format_timestamp(DateTime.from_unix!(us, :microsecond))

  def format_timestamp(_), do: nil

  # Intentional aborts (SIGABRT) are excluded — the backtrace tells the full story.
  @hardware_fault_signals ~w(SIGSEGV SIGILL SIGBUS SIGFPE)

  def hardware_fault_signal?(signal) when signal in @hardware_fault_signals, do: true
  def hardware_fault_signal?(_), do: false

  def format_registers(coredump), do: hardware_fault_field(coredump, :registers)
  def format_disassembly(coredump), do: hardware_fault_field(coredump, :disassembly)

  defp hardware_fault_field(%{signal: signal} = coredump, key) when is_binary(signal) do
    case Map.get(coredump, key) do
      value when is_binary(value) -> if hardware_fault_signal?(signal), do: value
      _ -> nil
    end
  end

  defp hardware_fault_field(_, _), do: nil

  def format_coredump_backtrace(%{threads: [thread | _]}) do
    frames = thread[:frames] || []
    shown = Enum.take(frames, @max_backtrace_frames)
    remaining = length(frames) - length(shown)
    backtrace = ToastTest.Enrichment.Coredump.format_backtrace(shown)
    suffix = if remaining > 0, do: "\n...", else: ""
    backtrace <> suffix
  end

  def format_coredump_backtrace(_), do: nil

  def format_coredump_path(%{core_path: path}) when is_binary(path), do: "Coredump: #{path}"
  def format_coredump_path(%{coredumps: [coredump | _]}), do: format_coredump_path(coredump)
  def format_coredump_path(_), do: nil

  def format_log_path(%{log_file: path}) when is_binary(path), do: "Log: #{path}"
  def format_log_path(_), do: nil
end
