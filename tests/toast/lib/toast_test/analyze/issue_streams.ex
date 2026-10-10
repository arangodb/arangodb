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

defmodule ToastTest.Analyze.IssueStreams do
  @moduledoc """
  Shared operations for time-windowed, server-scoped data streams.

  Provides server filtering, time window computation, event extraction,
  and k-way merge of multiple sorted streams. Used by both log analysis
  and traffic analysis.
  """

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

  @display_padding %{
    crash: {-5_000, 0},
    infrastructure: {-5_000, 0},
    test_failure: {-100, 100},
    sanitizer_report: {-100, 100}
  }

  @doc "Compute the display window for a given issue using its `:time_bounds`."
  def display_window(%{time_bounds: nil}, _window_spec), do: nil

  def display_window(%{time_bounds: {start_us, end_us}, type: type}, nil) do
    TimeWindows.pad(start_us, end_us, type, @display_padding)
  end

  def display_window(%{time_bounds: {start_us, end_us}}, {_before_ms, _after_ms} = padding) do
    TimeWindows.pad(start_us, end_us, padding)
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

  # --- Extract events ---

  @doc "Filter events by display window (microsecond timestamps)."
  def extract_events(events, {start_us, end_us}) do
    Enum.filter(events, fn event ->
      event.timestamp >= start_us and event.timestamp <= end_us
    end)
  end

  # --- Merge streams ---

  @doc """
  K-way merge of multiple tagged, pre-sorted streams into a single
  chronological stream.

  Each stream is a `{tag, [entry]}` tuple where entries are maps with
  either a `:time` or `:timestamp` key (microseconds). The tag is opaque
  — it can be a server ID string, `:event`, `:traffic`, or any term.

  Returns `[{tag, entry}]` in chronological order across all streams.
  Empty streams are filtered out.
  """
  @spec merge([{tag, [entry]}]) :: [{tag, entry}]
        when tag: term(), entry: map()
  def merge([]), do: []

  def merge([{tag, entries}]) do
    Enum.map(entries, &{tag, &1})
  end

  def merge(streams) do
    streams
    |> Enum.reject(fn {_, entries} -> entries == [] end)
    |> case do
      [] -> []
      [single] -> merge([single])
      multi -> k_way_merge(multi, [])
    end
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
