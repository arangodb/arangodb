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

  # --- Extract (main entry point) ---

  @doc """
  Filter servers by `server_filter`, then filter stored log lines by the
  given display window. Returns `[{server_id, filtered_lines}]` sorted by
  server ID.

  `servers` is `%{server_id => %{logs: [{start, end, lines_string}], ...}}`.
  `window` is `{DateTime.t(), DateTime.t()}` as returned by `display_window/2`.
  """
  def extract(servers, {win_start, win_end}, server_filter) do
    start_str = DateTime.to_iso8601(win_start)
    end_str = DateTime.to_iso8601(win_end)

    servers
    |> Enum.filter(fn {server_id, _} -> server_matches?(server_id, server_filter) end)
    |> Enum.flat_map(fn {server_id, meta} ->
      lines =
        (meta[:logs] || [])
        |> Enum.map(fn {_start, _end, lines_string} ->
          filter_lines(lines_string, {start_str, end_str})
        end)
        |> Enum.reject(&(&1 == ""))
        |> Enum.join("\n")

      if lines == "", do: [], else: [{server_id, lines}]
    end)
    |> Enum.sort_by(&elem(&1, 0))
  end

  # --- Line filtering ---

  @doc """
  Filter individual log lines within a stored window. Uses plain string
  comparison on the ISO 8601 timestamp prefix.
  """
  def filter_lines(lines_string, {start_str, end_str}) do
    lines_string
    |> String.split("\n")
    |> do_filter_lines(start_str, end_str, false, [])
    |> Enum.reverse()
    |> Enum.join("\n")
  end

  defp do_filter_lines([], _start, _end_s, _prev, acc), do: acc

  defp do_filter_lines([line | rest], start_str, end_str, prev_included, acc) do
    case extract_timestamp(line) do
      nil ->
        if prev_included do
          do_filter_lines(rest, start_str, end_str, true, [line | acc])
        else
          do_filter_lines(rest, start_str, end_str, false, acc)
        end

      ts ->
        if ts >= start_str and ts <= end_str do
          do_filter_lines(rest, start_str, end_str, true, [line | acc])
        else
          do_filter_lines(rest, start_str, end_str, false, acc)
        end
    end
  end

  # --- Merge streams ---

  @doc """
  K-way merge of pre-sorted per-server log lines into a single chronological
  stream. Non-timestamped continuation lines stay with their preceding line.
  """
  def merge_streams([]), do: []

  def merge_streams([{server_id, lines_string}]) do
    lines_string |> String.split("\n") |> Enum.map(&{server_id, &1})
  end

  def merge_streams(streams) do
    parsed =
      streams
      |> Enum.map(fn {server_id, lines_string} ->
        {server_id, group_with_continuations(String.split(lines_string, "\n"))}
      end)
      |> Enum.reject(fn {_, groups} -> groups == [] end)

    k_way_merge(parsed, [])
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

  @doc "Apply tags, colors, and alignment to produce the final output."
  def format_merged([], _color_enabled), do: ""

  def format_merged(merged, color_enabled) do
    servers = merged |> Enum.map(&elem(&1, 0)) |> Enum.uniq()

    if length(servers) == 1 do
      merged
      |> Enum.map(fn {_server_id, line} -> format_line_level(line, color_enabled) end)
      |> Enum.join("\n")
    else
      tag_map = Map.new(servers, &{&1, server_tag(&1)})
      color_map = Map.new(servers, &{&1, server_color(&1)})
      max_tag_len = tag_map |> Map.values() |> Enum.map(&String.length/1) |> Enum.max()

      merged
      |> Enum.map(fn {server_id, line} ->
        tag = String.pad_trailing(tag_map[server_id], max_tag_len)

        if color_enabled do
          color_code = color_map[server_id]
          level_extra = level_emphasis(line)
          "\e[38;5;#{color_code}m#{level_extra}[#{tag}] #{line}\e[0m"
        else
          "[#{tag}] #{line}"
        end
      end)
      |> Enum.join("\n")
    end
  end

  defp format_line_level(line, false), do: line

  defp format_line_level(line, true) do
    case level_emphasis(line) do
      "" -> line
      extra -> "#{extra}#{line}\e[0m"
    end
  end

  # ArangoDB log format: "2026-...Z [pid] LEVEL [topic] ..."
  defp level_emphasis(line) do
    case extract_log_level(line) do
      level when level in ~w(ERROR FATAL) -> IO.ANSI.inverse()
      "WARNING" -> IO.ANSI.bright()
      _ -> ""
    end
  end

  defp extract_log_level(line) do
    # Match: timestamp [pid] LEVEL ...
    case Regex.run(~r/^\S+ \[\S+\] \S+ (\w+)/, line) do
      [_, level] -> level
      _ -> nil
    end
  end

  # --- Private helpers ---

  defp derive_role(server_id), do: server_id |> derive_role_and_num() |> elem(0)

  defp derive_role_and_num(server_id) do
    # Server IDs follow either "<prefix>-<role>-<index>" (e.g., "toast-cluster-643-coordinator-0")
    # or simple "<role><index>" (e.g., "coordinator1", "single").
    segments = String.split(server_id, "-")

    case Enum.find_index(segments, &(&1 in @known_roles)) do
      nil ->
        # Simple format: strip trailing digits for role
        {String.replace(server_id, ~r/\d+$/, ""),
         Regex.run(~r/\d+$/, server_id, capture: :first) |> List.wrap() |> Enum.at(0, "")}

      idx ->
        role = Enum.at(segments, idx)
        num = Enum.slice(segments, (idx + 1)..-1//1) |> Enum.join("-")
        {role, num}
    end
  end

  defp extract_timestamp(line) do
    case String.split(line, " ", parts: 2) do
      [<<_y::4-bytes, "-", _m::2-bytes, "-", _d::2-bytes, "T", _::binary>> = ts | _]
      when byte_size(ts) >= 20 ->
        ts

      _ ->
        nil
    end
  end

  defp group_with_continuations(lines) do
    lines
    |> Enum.reduce([], fn line, acc ->
      ts = extract_timestamp(line)

      case {ts, acc} do
        {nil, [{prev_ts, prev_lines} | rest]} ->
          [{prev_ts, [line | prev_lines]} | rest]

        {nil, []} ->
          []

        {ts, _} ->
          [{ts, [line]} | acc]
      end
    end)
    |> Enum.map(fn {ts, lines} -> {ts, Enum.reverse(lines)} end)
    |> Enum.reverse()
  end

  defp k_way_merge([], acc), do: Enum.reverse(acc)

  defp k_way_merge(streams, acc) do
    {_, min_idx} =
      streams
      |> Enum.with_index()
      |> Enum.min_by(fn {{_server_id, [{ts, _lines} | _]}, _idx} -> ts end)

    {server_id, [{_ts, lines} | rest_groups]} = Enum.at(streams, min_idx)

    new_entries = Enum.map(lines, &{server_id, &1})

    new_streams =
      if rest_groups == [] do
        List.delete_at(streams, min_idx)
      else
        List.replace_at(streams, min_idx, {server_id, rest_groups})
      end

    k_way_merge(new_streams, Enum.reverse(new_entries) ++ acc)
  end

  defp parse_num(""), do: 0
  defp parse_num(s), do: String.to_integer(s)
end
