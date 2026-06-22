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

defmodule ToastTest.Traffic.Analysis do
  @moduledoc """
  Filtering, formatting, and server resolution for captured HTTP traffic entries.
  """

  alias ToastTest.Formatting.Utils

  @default_body_limit 200

  @type display_entry :: %{
          :pair_id => non_neg_integer() | nil,
          :pair_color => non_neg_integer() | nil,
          optional(atom()) => term()
        }

  @doc "Annotate an entry with {server_id, direction}."
  def annotate_server(%{src: {_, src_port}, dst: {_, dst_port}}, server_ports) do
    cond do
      Map.has_key?(server_ports, dst_port) ->
        {Map.get(server_ports, dst_port), :request}

      Map.has_key?(server_ports, src_port) ->
        {Map.get(server_ports, src_port), :response}

      true ->
        {nil, :unknown}
    end
  end

  # --- Pair color assignment ---

  # Muted 256-color palette for traffic pair IDs — chosen to be distinct from
  # the log server palettes (blue/teal for coordinators, warm for dbservers).
  @pair_colors [139, 68, 176, 136, 63, 170, 97]

  @doc """
  Assign a pair color index to each traffic entry based on {stream_id, request/response_number}.
  Returns entries with an added `:pair_id` and `:pair_color` field.
  """
  @spec assign_pair_colors([ToastTest.Traffic.Extraction.traffic_entry()]) :: [display_entry()]
  def assign_pair_colors(entries) do
    {colored, _state} =
      Enum.map_reduce(entries, %{next_id: 1, keys: %{}}, fn entry, state ->
        case pair_key(entry) do
          nil ->
            {Map.merge(entry, %{pair_id: nil, pair_color: nil}), state}

          key ->
            case state.keys[key] do
              nil ->
                id = state.next_id
                color = Enum.at(@pair_colors, rem(id - 1, length(@pair_colors)))
                entry = Map.merge(entry, %{pair_id: id, pair_color: color})
                {entry, %{state | next_id: id + 1, keys: Map.put(state.keys, key, {id, color})}}

              {id, color} ->
                {Map.merge(entry, %{pair_id: id, pair_color: color}), state}
            end
        end
      end)

    colored
  end

  defp pair_key(%{stream_id: sid, request_number: n}) when sid != nil and n != nil,
    do: {sid, n}

  defp pair_key(%{stream_id: sid, response_number: n}) when sid != nil and n != nil,
    do: {sid, n}

  defp pair_key(_), do: nil

  # --- Interesting headers ---

  @interesting_headers ~w(content-type content-length authorization x-arango)

  @doc "Filter headers to only the interesting ones, or return all if `all_headers` is true."
  def filter_headers(headers, true), do: headers

  def filter_headers(headers, _all) do
    Enum.filter(headers, fn header ->
      lower = String.downcase(header)
      Enum.any?(@interesting_headers, &String.starts_with?(lower, &1))
    end)
  end

  # --- Entry formatting ---

  @doc """
  Return `{summary, detail | nil}` for an entry.
  `summary` is the one-line request/response description.
  `detail` is the formatted headers + body (or nil if neither is present).
  """
  def format_entry_parts(entry, format_opts \\ %{}) do
    summary = format_summary(entry)
    headers = format_headers(entry, format_opts)
    body = format_body(entry, format_opts)

    detail =
      case [headers, body] |> Enum.reject(&is_nil/1) do
        [] -> nil
        parts -> Enum.join(parts, "\n")
      end

    {summary, detail}
  end

  defp format_summary(entry) do
    pair_tag = if entry[:pair_id], do: "[#{entry[:pair_id]}] ", else: ""

    cond do
      entry.method != nil ->
        "#{pair_tag}→ #{entry.method} #{entry.uri}"

      entry.status != nil ->
        "#{pair_tag}#{format_response(entry)}"

      true ->
        "#{pair_tag}? (no method or status)"
    end
  end

  defp format_response(entry) do
    ct = format_content_type(entry.content_type)
    rt = format_response_time(entry[:response_time])

    detail_parts =
      [ct, format_body_size(entry.body)]
      |> Enum.reject(&is_nil/1)

    status_line =
      case detail_parts do
        [] -> "← #{entry.status}"
        _ -> "← #{entry.status} (#{Enum.join(detail_parts, ", ")})"
      end

    if rt, do: status_line <> " #{rt}", else: status_line
  end

  defp format_response_time(nil), do: nil

  defp format_response_time(seconds) when seconds < 0.001,
    do: "[#{round(seconds * 1_000_000)}µs]"

  defp format_response_time(seconds) when seconds < 1.0,
    do: "[#{Float.round(seconds * 1000, 1)}ms]"

  defp format_response_time(seconds),
    do: "[#{Float.round(seconds, 2)}s]"

  defp format_content_type(nil), do: nil
  defp format_content_type(ct), do: if(vpack?(ct), do: "VPack", else: ct)

  defp format_body_size(nil), do: nil
  defp format_body_size(body), do: "#{byte_size(body)} bytes"

  defp format_headers(%{headers: headers}, opts) when is_list(headers) and headers != [] do
    all = Map.get(opts, :all_headers, false)
    filtered = filter_headers(headers, all)

    case filtered do
      [] -> nil
      _ -> Utils.indent(Enum.join(filtered, "\n"))
    end
  end

  defp format_headers(_, _), do: nil

  # --- Body formatting ---

  defp format_body(%{body: nil}, _opts), do: nil
  defp format_body(%{body: <<>>}, _opts), do: nil

  defp format_body(%{body: body, content_type: ct}, opts) do
    limit = Map.get(opts, :limit, @default_body_limit)
    raw = Map.get(opts, :raw, false)

    {text, truncated} =
      if vpack?(ct) and not raw do
        format_vpack_body(body, limit)
      else
        format_text_or_hex(body, limit)
      end

    suffix = if truncated, do: " …", else: ""
    Utils.indent(text <> suffix)
  end

  defp vpack?(nil), do: false
  defp vpack?(ct), do: String.contains?(ct, "velocypack")

  defp format_vpack_body(body, limit) do
    case VelocyPack.decode(body) do
      {:ok, decoded} ->
        text = inspect(decoded, pretty: true, width: 120)
        Utils.truncate(text, limit)

      {:error, _} ->
        format_hex(body, limit)
    end
  end

  defp format_text_or_hex(body, limit) do
    if String.printable?(body) do
      Utils.truncate(body, limit)
    else
      format_hex(body, limit)
    end
  end

  defp format_hex(body, limit) do
    {bytes, truncated} = truncate_bytes(body, limit)
    {Base.encode16(bytes, case: :lower), truncated}
  end

  defp truncate_bytes(body, :unlimited), do: {body, false}

  defp truncate_bytes(body, limit) do
    if byte_size(body) <= limit, do: {body, false}, else: {binary_part(body, 0, limit), true}
  end

  @doc "Filter traffic entries by time window and optional filters, sorted by timestamp."
  def extract(entries, server_ports, {start_us, end_us}, opts \\ []) do
    server_filter = Keyword.get(opts, :server_filter)
    method_filter = Keyword.get(opts, :method_filter)
    endpoint_filter = Keyword.get(opts, :endpoint_filter)
    status_filter = Keyword.get(opts, :status_filter)

    # Build set of allowed server IDs when server_filter is present
    server_ids =
      if server_filter do
        MapSet.new(server_filter)
      end

    entries
    |> Enum.filter(fn entry ->
      in_time_window?(entry, start_us, end_us) and
        passes_server_filter?(entry, server_ports, server_ids) and
        passes_method_filter?(entry, method_filter) and
        passes_endpoint_filter?(entry, endpoint_filter) and
        passes_status_filter?(entry, status_filter)
    end)
    |> Enum.sort_by(& &1.timestamp)
  end

  defp in_time_window?(%{timestamp: ts}, start_us, end_us) do
    ts >= start_us and ts <= end_us
  end

  defp passes_server_filter?(_entry, _server_ports, nil), do: true

  defp passes_server_filter?(%{src: {_, src_port}, dst: {_, dst_port}}, server_ports, server_ids) do
    src_server = Map.get(server_ports, src_port)
    dst_server = Map.get(server_ports, dst_port)

    (src_server != nil and MapSet.member?(server_ids, src_server)) or
      (dst_server != nil and MapSet.member?(server_ids, dst_server))
  end

  defp passes_method_filter?(_entry, nil), do: true
  defp passes_method_filter?(%{method: nil}, _filter), do: true
  defp passes_method_filter?(%{method: method}, filter), do: method in filter

  defp passes_endpoint_filter?(_entry, nil), do: true
  defp passes_endpoint_filter?(%{uri: nil}, _filter), do: true

  defp passes_endpoint_filter?(%{uri: uri}, filter) do
    Enum.any?(filter, &String.contains?(uri, &1))
  end

  defp passes_status_filter?(_entry, nil), do: true
  defp passes_status_filter?(%{status: nil}, _filter), do: true

  defp passes_status_filter?(%{status: status}, {min, max}) do
    status >= min and status <= max
  end
end
