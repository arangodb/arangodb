################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule ToastTest.Analyze.Logs do
  @moduledoc """
  Log-specific data transformation: level filtering, ID exclusion, and
  extraction of log entries from chunked storage.

  For server filtering, time windows, event extraction, and stream merging,
  see `ToastTest.Analyze.IssueStreams`.
  """

  @levels [:trace, :debug, :info, :warning, :error, :fatal]
  @level_index Map.new(Enum.with_index(@levels))

  # --- Parsing ---

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

  # --- Extract ---

  @doc """
  Filter stored log entries by the given display window.
  Returns `[{{:server, server_id}, [entry]}]` sorted by server ID — the
  `{:server, _}` tag lets the stream merge dispatch on a closed set of tags.

  `servers` is a pre-filtered map/list of `{server_id => %{logs: [{start, end, [entry]}], ...}}`.
  `window` is `{Toast.timestamp(), Toast.timestamp()}` as returned by `IssueStreams.display_window/2`.

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

      if entries == [], do: [], else: [{{:server, server_id}, entries}]
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
end
