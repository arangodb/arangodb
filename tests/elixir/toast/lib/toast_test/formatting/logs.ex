defmodule ToastTest.Formatting.Logs do
  @moduledoc """
  Display formatting for server log output.

  Handles server tags, color assignment, and rendering of merged
  log streams into human-readable output. Data transformation
  (filtering, merging, windowing) lives in `ToastTest.LogAnalysis`.
  """

  @role_abbrevs %{
    coordinator: "CO",
    dbserver: "DB",
    agent: "AG",
    single: "SNG"
  }

  # Muted 256-color ANSI palettes per role
  @coordinator_colors [67, 103, 110, 66, 109, 60, 68, 102, 146]
  @dbserver_colors [137, 174, 95, 180, 130, 215, 101, 172, 144]
  @agent_colors [101, 138, 66, 144, 96]

  # --- Server tag ---

  @doc "Short uppercase tag for a server. `role` is an atom like `:coordinator`."
  def server_tag(server_id, role) do
    abbrev =
      @role_abbrevs[role] ||
        String.upcase(String.slice(Atom.to_string(role || :""), 0, 3))

    abbrev <> extract_instance_num(server_id)
  end

  # --- Server color ---

  @doc "256-color ANSI code for a role (atom) and instance number."
  def server_color(role, instance_num) do
    palette =
      case role do
        :coordinator -> @coordinator_colors
        :dbserver -> @dbserver_colors
        :single -> @dbserver_colors
        :agent -> @agent_colors
        _ -> @coordinator_colors
      end

    Enum.at(palette, rem(instance_num, length(palette)))
  end

  # --- Format merged output ---

  @doc """
  Format merged `[{server_id | :event, entry | event}]` into display lines.

  Options:
  - `event_detail` -- `:basic` (default) or `:full`
  - `server_roles` -- `%{server_id => atom()}` for tag/color derivation
  """
  def format_merged(merged, color_enabled, event_detail \\ :basic, server_roles \\ %{})

  def format_merged([], _color_enabled, _event_detail, _server_roles), do: ""

  def format_merged(merged, color_enabled, event_detail, server_roles) do
    servers = merged |> Enum.map(&elem(&1, 0)) |> Enum.uniq() |> Enum.reject(&(&1 == :event))
    has_events = Enum.any?(merged, &match?({:event, _}, &1))

    if length(servers) <= 1 and not has_events do
      merged
      |> Enum.map_join("\n", fn
        {_server_id, entry} -> format_entry(entry, color_enabled)
      end)
    else
      {tag_map, color_map} =
        Enum.reduce(servers, {%{}, %{}}, fn sid, {tags, colors} ->
          role = server_roles[sid]
          instance_num_str = extract_instance_num(sid)
          num = parse_instance_num(instance_num_str)

          tag = server_tag(sid, role)

          {Map.put(tags, sid, tag), Map.put(colors, sid, server_color(role, num))}
        end)

      max_tag_len =
        case Map.values(tag_map) do
          [] -> 0
          tags -> tags |> Enum.map(&String.length/1) |> Enum.max()
        end

      merged
      |> Enum.map_join("\n", fn
        {:event, event} ->
          format_event_line(event, max_tag_len, event_detail)

        {server_id, entry} ->
          format_tagged_entry(server_id, entry, tag_map, color_map, max_tag_len, color_enabled)
      end)
    end
  end

  defp format_event_line(event, max_tag_len, event_detail) do
    padding = String.duplicate(" ", max_tag_len + 2)
    ts = event.timestamp |> DateTime.from_unix!(:microsecond) |> DateTime.to_iso8601()
    line = "#{padding} #{ts} #{format_event(event)}"

    if event_detail == :full do
      line <> "\n#{padding}   #{inspect(event, pretty: true, width: 120)}"
    else
      line
    end
  end

  defp format_tagged_entry(server_id, entry, tag_map, color_map, max_tag_len, color_enabled) do
    tag = String.pad_trailing(tag_map[server_id], max_tag_len)
    line = format_entry_line(entry)

    if color_enabled do
      color_code = color_map[server_id]
      level_extra = level_emphasis(entry)
      "\e[38;5;#{color_code}m#{level_extra}[#{tag}] #{line}\e[0m"
    else
      "[#{tag}] #{line}"
    end
  end

  def format_event(%{event: :server_started, server_id: sid, pid: pid}),
    do: ">>> server_started #{sid} (pid=#{pid})"

  def format_event(%{event: :server_stopped, server_id: sid}),
    do: ">>> server_stopped #{sid}"

  def format_event(%{event: :server_crashed, server_id: sid, pid: pid, signal: sig}),
    do: ">>> server_crashed #{sid} (pid=#{pid}, signal=#{sig})"

  def format_event(%{event: :server_killed, server_id: sid}),
    do: ">>> server_killed #{sid}"

  def format_event(%{event: :server_paused, server_id: sid}),
    do: ">>> server_paused #{sid}"

  def format_event(%{event: :server_resumed, server_id: sid}),
    do: ">>> server_resumed #{sid}"

  def format_event(%{event: :test_started, module: mod, name: name}),
    do: ">>> test_started #{inspect(mod)} > #{name}"

  def format_event(%{event: :test_finished, module: mod, name: name, outcome: outcome}),
    do: ">>> test_finished #{inspect(mod)} > #{name} (#{outcome})"

  def format_event(%{event: :module_started, module: mod}),
    do: ">>> module_started #{inspect(mod)}"

  def format_event(%{event: :module_finished, module: mod}),
    do: ">>> module_finished #{inspect(mod)}"

  def format_event(%{event: :deployment_starting, deployment_id: did, mode: mode}),
    do: ">>> deployment_starting #{did} (#{mode})"

  def format_event(%{event: :deployment_started, deployment_id: did}),
    do: ">>> deployment_started #{did}"

  def format_event(%{event: :deployment_stopped, deployment_id: did}),
    do: ">>> deployment_stopped #{did}"

  def format_event(%{event: :timeout_kill, reason: reason}),
    do: ">>> timeout_kill #{reason}"

  def format_event(%{event: :server_identified, server_id: sid, arango_id: aid}),
    do: ">>> server_identified #{sid} => #{aid}"

  def format_event(%{event: :custom, kind: kind, payload: payload}) when payload == %{},
    do: ">>> custom:#{kind}"

  def format_event(%{event: :custom, kind: kind, payload: payload}),
    do: ">>> custom:#{kind} #{format_payload(payload)}"

  def format_event(%{event: name}),
    do: ">>> #{name}"

  defp format_payload(payload) do
    payload
    |> Enum.map_join(" ", fn {k, v} -> "#{k}: #{inspect(v)}" end)
  end

  @doc "Format a single log entry as a human-readable line."
  def format_entry(entry, color_enabled) do
    line = format_entry_line(entry)

    if color_enabled do
      case level_emphasis(entry) do
        "" -> line
        extra -> "#{extra}#{line}\e[0m"
      end
    else
      line
    end
  end

  defp format_entry_line(entry) do
    ts = entry.time |> DateTime.from_unix!(:microsecond) |> DateTime.to_iso8601()
    level = format_level(entry[:level])
    id = if entry[:id], do: " [#{entry.id}]", else: ""
    topic = if entry[:topic], do: " {#{entry.topic}}", else: ""
    pid = if entry[:pid], do: " [#{entry.pid}]", else: ""

    "#{ts}#{pid} #{level}#{id}#{topic} #{entry.message}"
  end

  defp format_level(:fatal), do: "FATAL"
  defp format_level(:error), do: "ERROR"
  defp format_level(:warning), do: "WARN"
  defp format_level(:info), do: "INFO"
  defp format_level(:debug), do: "DEBUG"
  defp format_level(:trace), do: "TRACE"
  defp format_level(_), do: "???"

  defp level_emphasis(%{level: level}) when level in [:error, :fatal], do: IO.ANSI.inverse()
  defp level_emphasis(%{level: :warning}), do: IO.ANSI.bright()
  defp level_emphasis(_), do: ""

  # --- Private helpers ---

  # Extract trailing digits from a server ID as the instance number string.
  # "cluster-00-coordinator-0" -> "0", "single-00" -> "00", "single" -> ""
  defp extract_instance_num(server_id) do
    case Regex.run(~r/(\d+)$/, server_id) do
      [_, num] -> num
      nil -> ""
    end
  end

  defp parse_instance_num(""), do: 0
  defp parse_instance_num(s), do: String.to_integer(s)
end
