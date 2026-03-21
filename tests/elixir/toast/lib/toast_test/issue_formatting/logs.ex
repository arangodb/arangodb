defmodule ToastTest.IssueFormatting.Logs do
  @moduledoc false

  @known_roles ~w(agent coordinator dbserver single)
  @default_exclude_roles ["agent"]

  @type_defaults %{
    crash: {-20, 0},
    timeout: {-10, 0},
    test_failure: {-1, 1},
    sanitizer_report: {-5, 1}
  }

  @role_abbrevs %{
    "coordinator" => "CO",
    "dbserver" => "DB",
    "agent" => "AG",
    "single" => "SNG"
  }

  # Muted 256-color ANSI palettes per role
  @coordinator_colors [67, 103, 110, 66, 109, 60, 68, 102, 146]
  @dbserver_colors [137, 174, 95, 180, 130, 215, 101, 172, 144]
  @agent_colors [101, 138, 66, 144, 96]

  # --- Parsing ---

  @doc "Parse `--log-servers` value. `nil` → default (exclude agents)."
  def parse_server_filter(nil) do
    (@known_roles -- @default_exclude_roles) |> Enum.map(&{:role, &1})
  end

  def parse_server_filter("all"), do: :all

  def parse_server_filter(spec) when is_binary(spec) do
    spec
    |> String.split(",", trim: true)
    |> Enum.map(fn filter ->
      filter = String.trim(filter)
      if filter in @known_roles, do: {:role, filter}, else: {:prefix, filter}
    end)
  end

  @doc "Parse `--log-window` value. `nil` → use type-specific defaults."
  def parse_window_spec(nil), do: nil

  def parse_window_spec(spec) when is_binary(spec) do
    case String.split(spec, ",", parts: 2) do
      [before, after_s] ->
        {String.to_integer(String.trim(before)), String.to_integer(String.trim(after_s))}

      [before] ->
        {String.to_integer(String.trim(before)), 0}
    end
  end

  # --- Server matching ---

  @doc "Check if a server ID passes the filter."
  def server_matches?(_server_id, :all), do: true

  def server_matches?(server_id, filters) when is_list(filters) do
    Enum.any?(filters, fn
      {:role, role} -> derive_role(server_id) == role
      {:prefix, prefix} -> String.starts_with?(server_id, prefix)
    end)
  end

  # --- Display window ---

  @doc "Compute the display window for a given issue using its `:time_bounds`."
  def display_window(%{time_bounds: nil}, _window_spec), do: nil

  def display_window(%{time_bounds: {start_dt, end_dt}, type: type}, nil) do
    {before_s, after_s} = Map.fetch!(@type_defaults, type)
    {DateTime.add(start_dt, before_s, :second), DateTime.add(end_dt, after_s, :second)}
  end

  def display_window(%{time_bounds: {start_dt, end_dt}}, {before_s, after_s}) do
    {DateTime.add(start_dt, before_s, :second), DateTime.add(end_dt, after_s, :second)}
  end

  @doc "Return sorted list of server IDs that pass the filter."
  def matching_servers(servers, server_filter) do
    servers
    |> Map.keys()
    |> Enum.filter(&server_matches?(&1, server_filter))
    |> Enum.sort()
  end

  # --- Extract ---

  @doc """
  Filter servers by `server_filter`, then filter stored log entries by the
  given display window. Returns `[{server_id, [entry]}]` sorted by server ID.

  `servers` is `%{server_id => %{logs: [{start, end, [entry]}], ...}}`.
  `window` is `{DateTime.t(), DateTime.t()}` as returned by `display_window/2`.
  """
  def extract(servers, {win_start, win_end}, server_filter) do
    start_us = DateTime.to_unix(win_start, :microsecond)
    end_us = DateTime.to_unix(win_end, :microsecond)

    servers
    |> Enum.filter(fn {server_id, _} -> server_matches?(server_id, server_filter) end)
    |> Enum.flat_map(fn {server_id, meta} ->
      entries =
        (meta[:logs] || [])
        |> Enum.flat_map(fn {_start, _end, entries} ->
          Enum.filter(entries, fn entry ->
            entry.time >= start_us and entry.time <= end_us
          end)
        end)

      if entries == [], do: [], else: [{server_id, entries}]
    end)
    |> Enum.sort_by(&elem(&1, 0))
  end

  # --- Extract events ---

  @doc "Filter events by display window (microsecond timestamps)."
  def extract_events(events, {win_start, win_end}) do
    start_us = DateTime.to_unix(win_start, :microsecond)
    end_us = DateTime.to_unix(win_end, :microsecond)

    Enum.filter(events, fn event ->
      event.timestamp >= start_us and event.timestamp <= end_us
    end)
  end

  # --- Merge streams ---

  @doc """
  K-way merge of pre-sorted per-server log entry lists into a single
  chronological stream. Returns `[{server_id | :event, entry | event}]`.

  An optional `events` list can be provided to interleave EventStore events
  with server log entries. Events use `:timestamp` for ordering while log
  entries use `:time`.
  """
  def merge_streams(streams, events \\ [])

  def merge_streams([], []), do: []

  def merge_streams([], events) do
    Enum.map(events, &{:event, &1})
  end

  def merge_streams([{server_id, entries}], []) do
    Enum.map(entries, &{server_id, &1})
  end

  def merge_streams(streams, events) do
    event_stream =
      if events == [], do: [], else: [{:event, events}]

    (streams ++ event_stream)
    |> Enum.reject(fn {_, entries} -> entries == [] end)
    |> k_way_merge([])
  end

  # --- Server tag ---

  @doc "Derive a short uppercase tag from a server ID."
  def server_tag(server_id) do
    {role, num} = derive_role_and_num(server_id)
    abbrev = Map.get(@role_abbrevs, role, String.upcase(String.slice(role, 0, 3)))
    abbrev <> num
  end

  # --- Server color ---

  @doc "Get 256-color ANSI code for a server ID."
  def server_color(server_id) do
    {role, num_str} = derive_role_and_num(server_id)
    num = parse_num(num_str)

    palette =
      case role do
        "coordinator" -> @coordinator_colors
        "dbserver" -> @dbserver_colors
        "agent" -> @agent_colors
        _ -> @coordinator_colors
      end

    Enum.at(palette, rem(num, length(palette)))
  end

  # --- Format merged output ---

  @doc """
  Format merged `[{server_id | :event, entry | event}]` into display lines.

  `event_detail` controls how events are rendered:
  - `:basic` — one-line summary (default)
  - `:full` — one-line summary followed by the full event map
  """
  def format_merged(merged, color_enabled, event_detail \\ :basic)

  def format_merged([], _color_enabled, _event_detail), do: ""

  def format_merged(merged, color_enabled, event_detail) do
    servers = merged |> Enum.map(&elem(&1, 0)) |> Enum.uniq() |> Enum.reject(&(&1 == :event))

    if length(servers) <= 1 do
      merged
      |> Enum.map(fn
        {:event, event} -> format_event_line(event, event_detail)
        {_server_id, entry} -> format_entry(entry, color_enabled)
      end)
      |> Enum.join("\n")
    else
      tag_map = Map.new(servers, &{&1, server_tag(&1)})
      color_map = Map.new(servers, &{&1, server_color(&1)})
      max_tag_len = tag_map |> Map.values() |> Enum.map(&String.length/1) |> Enum.max()

      merged
      |> Enum.map(fn
        {:event, event} ->
          padding = String.duplicate(" ", max_tag_len + 2)
          ts = event.timestamp |> DateTime.from_unix!(:microsecond) |> DateTime.to_iso8601()
          line = "#{padding} #{ts} #{format_event(event)}"

          if event_detail == :full do
            line <> "\n#{padding}   #{inspect(event, pretty: true, width: 120)}"
          else
            line
          end

        {server_id, entry} ->
          tag = String.pad_trailing(tag_map[server_id], max_tag_len)
          line = format_entry_line(entry)

          if color_enabled do
            color_code = color_map[server_id]
            level_extra = level_emphasis(entry)
            "\e[38;5;#{color_code}m#{level_extra}[#{tag}] #{line}\e[0m"
          else
            "[#{tag}] #{line}"
          end
      end)
      |> Enum.join("\n")
    end
  end

  @doc "Format a single event as a `>>> event_name details` string."
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

  def format_event(%{event: name}),
    do: ">>> #{name}"

  defp format_event_line(event, detail) do
    ts = event.timestamp |> DateTime.from_unix!(:microsecond) |> DateTime.to_iso8601()
    line = "#{ts} #{format_event(event)}"

    if detail == :full do
      line <> "\n  #{inspect(event, pretty: true, width: 120)}"
    else
      line
    end
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

  defp derive_role(server_id), do: server_id |> derive_role_and_num() |> elem(0)

  defp derive_role_and_num(server_id) do
    segments = String.split(server_id, "-")

    case Enum.find_index(segments, &(&1 in @known_roles)) do
      nil ->
        {String.replace(server_id, ~r/\d+$/, ""),
         Regex.run(~r/\d+$/, server_id, capture: :first) |> List.wrap() |> Enum.at(0, "")}

      idx ->
        role = Enum.at(segments, idx)
        num = Enum.slice(segments, (idx + 1)..-1//1) |> Enum.join("-")
        {role, num}
    end
  end

  defp k_way_merge([], acc), do: Enum.reverse(acc)

  defp k_way_merge(streams, acc) do
    {_, min_idx} =
      streams
      |> Enum.with_index()
      |> Enum.min_by(fn {{_server_id, [entry | _]}, _idx} -> entry_time(entry) end)

    {server_id, [entry | rest_entries]} = Enum.at(streams, min_idx)

    new_streams =
      if rest_entries == [] do
        List.delete_at(streams, min_idx)
      else
        List.replace_at(streams, min_idx, {server_id, rest_entries})
      end

    k_way_merge(new_streams, [{server_id, entry} | acc])
  end

  defp entry_time(%{time: t}), do: t
  defp entry_time(%{timestamp: t}), do: t

  defp parse_num(""), do: 0
  defp parse_num(s), do: String.to_integer(s)
end
