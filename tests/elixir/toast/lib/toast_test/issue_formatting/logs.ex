defmodule ToastTest.IssueFormatting.Logs do
  @moduledoc false

  @levels [:trace, :debug, :info, :warning, :error, :fatal]
  @level_index Map.new(Enum.with_index(@levels))

  @known_roles ~w(agent coordinator dbserver single)a
  @default_exclude_roles [:agent]

  @type_defaults %{
    crash: {-20, 0},
    timeout: {-10, 0},
    test_failure: {-1, 1},
    sanitizer_report: {-5, 1}
  }

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

  # --- Parsing ---

  @doc "Parse `--log-servers` value. `nil` -> default (exclude agents)."
  def parse_server_filter(nil) do
    (@known_roles -- @default_exclude_roles) |> Enum.map(&{:role, &1})
  end

  def parse_server_filter("all"), do: :all

  def parse_server_filter(spec) when is_binary(spec) do
    known_strings = Map.new(@known_roles, &{Atom.to_string(&1), &1})

    spec
    |> String.split(",", trim: true)
    |> Enum.map(fn filter ->
      filter = String.trim(filter)

      case known_strings[filter] do
        nil -> {:prefix, filter}
        role -> {:role, role}
      end
    end)
  end

  @doc "Parse `--log-window` value. `nil` -> use type-specific defaults."
  def parse_window_spec(nil), do: nil

  def parse_window_spec(spec) when is_binary(spec) do
    case String.split(spec, ",", parts: 2) do
      [before, after_s] ->
        {String.to_integer(String.trim(before)), String.to_integer(String.trim(after_s))}

      [before] ->
        {String.to_integer(String.trim(before)), 0}
    end
  end

  @doc "Parse `--log-exclude` value. `nil` -> no exclusions."
  def parse_exclude(nil), do: nil

  def parse_exclude(spec) when is_binary(spec) do
    spec
    |> String.split(",", trim: true)
    |> MapSet.new(&String.trim/1)
  end

  @doc "Parse `--log-min-level` value. `nil` -> no filtering."
  def parse_level_filter(nil), do: nil

  def parse_level_filter(spec) when is_binary(spec) do
    parts = String.split(spec, ",", trim: true)

    {global, topics} =
      Enum.reduce(parts, {nil, %{}}, fn part, {global, topics} ->
        part = String.trim(part)

        case String.split(part, "=", parts: 2) do
          [topic_str, level_str] ->
            level = parse_level!(level_str)
            topic = String.to_atom(topic_str)
            {global, Map.put(topics, topic, level)}

          [level_str] ->
            {parse_level!(level_str), topics}
        end
      end)

    %{global: global, topics: topics}
  end

  @known_level_strings Map.new(@levels, &{Atom.to_string(&1), &1})

  defp parse_level!(str) do
    str = str |> String.trim() |> String.downcase()

    @known_level_strings[str] ||
      Mix.raise(
        "Unknown log level: #{str}. Valid: #{Map.keys(@known_level_strings) |> Enum.join(", ")}"
      )
  end

  @doc "Check if a log entry passes the level filter."
  def level_passes?(%{level: entry_level} = entry, %{global: global, topics: topics}) do
    min_level = Map.get(topics, entry[:topic], global)

    min_level == nil or
      Map.get(@level_index, entry_level, 0) >= Map.fetch!(@level_index, min_level)
  end

  # Entries without a :level key, or nil filter — always pass
  def level_passes?(_entry, _filter), do: true

  # --- Server matching ---

  @doc "Check if a server passes the filter. `role` is an atom."
  def server_matches?(_server_id, :all, _role), do: true

  def server_matches?(server_id, filters, role) when is_list(filters) do
    Enum.any?(filters, fn
      {:role, filter_role} -> role == filter_role
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

  # --- Server filtering ---

  @doc "Filter servers by `server_filter`. Returns `[{server_id, meta}]`."
  def filter_servers(servers, server_filter) do
    Enum.filter(servers, fn {server_id, meta} ->
      server_matches?(server_id, server_filter, meta[:role])
    end)
  end

  @doc "Return sorted list of server IDs that pass the filter."
  def matching_servers(servers, server_filter) do
    servers
    |> filter_servers(server_filter)
    |> Enum.map(&elem(&1, 0))
    |> Enum.sort()
  end

  # --- Extract ---

  @doc """
  Filter stored log entries by the given display window.
  Returns `[{server_id, [entry]}]` sorted by server ID.

  `servers` is a pre-filtered map/list of `{server_id => %{logs: [{start, end, [entry]}], ...}}`.
  `window` is `{DateTime.t(), DateTime.t()}` as returned by `display_window/2`.

  Options:
  - `level_filter` — parsed level filter from `parse_level_filter/1`
  - `excluded_ids` — `MapSet` of log IDs to exclude, from `parse_exclude/1`
  """
  def extract(servers, {win_start, win_end}, opts \\ []) do
    start_us = DateTime.to_unix(win_start, :microsecond)
    end_us = DateTime.to_unix(win_end, :microsecond)
    level_filter = opts[:level_filter]
    excluded_ids = opts[:excluded_ids]

    servers
    |> Enum.flat_map(fn {server_id, meta} ->
      entries =
        (meta[:logs] || [])
        |> Enum.flat_map(fn {_start, _end, entries} ->
          Enum.filter(entries, fn entry ->
            entry.time >= start_us and entry.time <= end_us and
              level_passes?(entry, level_filter) and
              not id_excluded?(entry, excluded_ids)
          end)
        end)

      if entries == [], do: [], else: [{server_id, entries}]
    end)
    |> Enum.sort_by(&elem(&1, 0))
  end

  defp id_excluded?(_entry, nil), do: false
  defp id_excluded?(entry, excluded_ids), do: MapSet.member?(excluded_ids, entry[:id])

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
      |> Enum.map(fn
        {_server_id, entry} -> format_entry(entry, color_enabled)
      end)
      |> Enum.join("\n")
    else
      {tag_map, color_map} =
        Enum.reduce(servers, {%{}, %{}}, fn sid, {tags, colors} ->
          role = server_roles[sid]
          instance_num_str = extract_instance_num(sid)
          num = parse_instance_num(instance_num_str)

          tag =
            (@role_abbrevs[role] || String.upcase(String.slice(Atom.to_string(role || :""), 0, 3))) <>
              instance_num_str

          {Map.put(tags, sid, tag), Map.put(colors, sid, server_color(role, num))}
        end)

      max_tag_len =
        case Map.values(tag_map) do
          [] -> 0
          tags -> tags |> Enum.map(&String.length/1) |> Enum.max()
        end

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
  # "toast-cluster-31-coordinator-0" -> "0", "toast-670" -> "670", "single" -> ""
  defp extract_instance_num(server_id) do
    case Regex.run(~r/(\d+)$/, server_id) do
      [_, num] -> num
      nil -> ""
    end
  end

  defp parse_instance_num(""), do: 0
  defp parse_instance_num(s), do: String.to_integer(s)

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
end
