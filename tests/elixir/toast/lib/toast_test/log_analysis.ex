defmodule ToastTest.LogAnalysis do
  @moduledoc """
  Data transformation for server log analysis.

  Handles parsing of CLI filter options, server matching, time window
  computation, log entry extraction/filtering, and k-way merge of
  multiple sorted log streams.
  """

  @levels [:trace, :debug, :info, :warning, :error, :fatal]
  @level_index Map.new(Enum.with_index(@levels))

  @known_roles ~w(agent coordinator dbserver single)a
  @default_exclude_roles [:agent]

  alias ToastTest.Attribution.TimeWindows

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

  @usec_per_ms 1_000

  @doc "Compute the display window for a given issue using its `:time_bounds`."
  def display_window(%{time_bounds: nil}, _window_spec), do: nil

  def display_window(%{time_bounds: {start_us, end_us}, type: type}, nil) do
    TimeWindows.pad(start_us, end_us, type)
  end

  def display_window(%{time_bounds: {start_us, end_us}}, {before_ms, after_ms}) do
    {start_us + before_ms * @usec_per_ms, end_us + after_ms * @usec_per_ms}
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
  `window` is `{Toast.timestamp(), Toast.timestamp()}` as returned by `display_window/2`.

  Options:
  - `level_filter` — parsed level filter from `parse_level_filter/1`
  - `excluded_ids` — `MapSet` of log IDs to exclude, from `parse_exclude/1`
  """
  def extract(servers, {start_us, end_us}, opts \\ []) do
    level_filter = opts[:level_filter]
    excluded_ids = opts[:excluded_ids]

    servers
    |> Enum.flat_map(fn {server_id, meta} ->
      entries =
        filter_server_entries(meta[:logs] || [], start_us, end_us, level_filter, excluded_ids)

      if entries == [], do: [], else: [{server_id, entries}]
    end)
    |> Enum.sort_by(&elem(&1, 0))
  end

  defp filter_server_entries(log_chunks, start_us, end_us, level_filter, excluded_ids) do
    Enum.flat_map(log_chunks, fn {_start, _end, entries} ->
      Enum.filter(entries, fn entry ->
        entry.time >= start_us and entry.time <= end_us and
          level_passes?(entry, level_filter) and
          not id_excluded?(entry, excluded_ids)
      end)
    end)
  end

  defp id_excluded?(_entry, nil), do: false
  defp id_excluded?(entry, excluded_ids), do: MapSet.member?(excluded_ids, entry[:id])

  # --- Extract events ---

  @doc "Filter events by display window (microsecond timestamps)."
  def extract_events(events, {start_us, end_us}) do
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

  # --- Private helpers ---

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
