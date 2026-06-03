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

  @doc "Parse a comma-separated method filter spec into a list, or nil."
  def parse_method_filter(nil), do: nil
  def parse_method_filter(spec) when is_binary(spec), do: parse_csv(spec)

  @doc "Parse a comma-separated endpoint filter spec into a list, or nil."
  def parse_endpoint_filter(nil), do: nil
  def parse_endpoint_filter(spec) when is_binary(spec), do: parse_csv(spec)

  defp parse_csv(spec), do: spec |> String.split(",", trim: true) |> Enum.map(&String.trim/1)

  @doc "Parse a status filter spec into a {min, max} range, or nil."
  def parse_status_filter(nil), do: nil

  def parse_status_filter(spec) when is_binary(spec) do
    case String.split(spec, "-") do
      [single] -> {String.to_integer(single), String.to_integer(single)}
      [min, max] -> {String.to_integer(min), String.to_integer(max)}
    end
  end

  @doc "Resolve which server an entry belongs to by checking src then dst port."
  def resolve_server(%{src: {_, src_port}, dst: {_, dst_port}}, server_ports) do
    Map.get(server_ports, src_port) || Map.get(server_ports, dst_port)
  end

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

  @doc "Format an entry for display."
  def format_entry(entry) do
    cond do
      entry.method != nil ->
        "→ #{entry.method} #{entry.uri}"

      entry.status != nil ->
        format_response(entry)

      true ->
        "? (no method or status)"
    end
  end

  defp format_response(entry) do
    parts = ["← #{entry.status}"]

    ct = format_content_type(entry.content_type)

    detail_parts =
      [ct, format_body_size(entry.body)]
      |> Enum.reject(&is_nil/1)

    case detail_parts do
      [] -> Enum.join(parts)
      _ -> Enum.join(parts) <> " (#{Enum.join(detail_parts, ", ")})"
    end
  end

  defp format_content_type(nil), do: nil

  defp format_content_type(ct) do
    if String.contains?(ct, "velocypack"), do: "VPack", else: ct
  end

  defp format_body_size(nil), do: nil
  defp format_body_size(body), do: "#{byte_size(body)} bytes"

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
