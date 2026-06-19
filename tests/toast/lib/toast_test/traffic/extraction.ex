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

defmodule ToastTest.Traffic.Extraction do
  @moduledoc """
  Parses tshark `-T ek` (newline-delimited JSON) output into structured
  traffic entry maps.
  """

  require Logger

  @type traffic_entry :: %{
          timestamp: integer(),
          src: {String.t(), integer()},
          dst: {String.t(), integer()},
          method: String.t() | nil,
          uri: String.t() | nil,
          status: integer() | nil,
          content_type: String.t() | nil,
          body: binary() | nil,
          headers: [String.t()],
          stream_id: integer() | nil,
          request_number: integer() | nil,
          response_number: integer() | nil,
          response_time: float() | nil
        }

  @spec parse_tshark_output(String.t()) :: [traffic_entry()]
  def parse_tshark_output(json_lines) do
    json_lines
    |> String.split("\n")
    |> Enum.flat_map(&parse_line/1)
  end

  # Sub-dissectors that tshark applies to HTTP bodies, moving the data
  # out of http.file_data into their own protocol layer. Disabling them
  # keeps the raw body bytes in the HTTP layer where we can extract them.
  @disabled_subdissectors ~w(json data-text-lines xml urlencoded-form)

  @max_total_bytes 200 * 1024 * 1024
  @entry_overhead 200

  @doc """
  Run tshark on a pcap file and stream its output, parsing entries on the fly.
  Aborts tshark if accumulated entry data exceeds the size limit.
  """
  @spec extract(String.t()) ::
          {:ok, [traffic_entry()]}
          | {:error, :tshark_not_found | {:tshark_exited, non_neg_integer()}}
  def extract(pcap_path) do
    disable_args = Enum.flat_map(@disabled_subdissectors, &["--disable-protocol", &1])
    args = ["-r", pcap_path, "-T", "ek", "-Y", "http"] ++ disable_args

    case System.find_executable("tshark") do
      nil ->
        {:error, :tshark_not_found}

      tshark ->
        Logger.info("Running tshark on #{pcap_path}")

        port =
          Port.open({:spawn_executable, tshark}, [
            :binary,
            :exit_status,
            :use_stdio,
            :stderr_to_stdout,
            {:line, 1_048_576},
            {:args, args}
          ])

        case consume_port(port, %{entries: [], size: 0, count: 0, buffer: ""}) do
          {:ok, result} ->
            log_result(result)
            {:ok, result.entries}

          {:error, reason} ->
            {:error, reason}
        end
    end
  end

  defp consume_port(port, state) do
    receive do
      {^port, {:data, {:eol, line}}} ->
        full_line = state.buffer <> line
        state = %{state | buffer: ""}

        case parse_line(full_line) do
          [] ->
            consume_port(port, state)

          [entry] ->
            entry_size = @entry_overhead + byte_size(entry[:body] || <<>>)
            new_size = state.size + entry_size

            if new_size > @max_total_bytes do
              Logger.warning(
                "Traffic data limit reached (#{format_bytes(new_size)}, " <>
                  "limit #{format_bytes(@max_total_bytes)}) — aborting tshark, " <>
                  "keeping #{state.count} entries"
              )

              Port.close(port)
              drain_port(port)
              {:ok, finalize(state)}
            else
              consume_port(port, %{
                state
                | entries: [entry | state.entries],
                  size: new_size,
                  count: state.count + 1
              })
            end
        end

      {^port, {:data, {:noeol, chunk}}} ->
        consume_port(port, %{state | buffer: state.buffer <> chunk})

      {^port, {:exit_status, 0}} ->
        {:ok, finalize(state)}

      {^port, {:exit_status, code}} ->
        Logger.warning("tshark exited with code #{code} — traffic extraction failed")
        {:error, {:tshark_exited, code}}
    after
      300_000 ->
        Logger.warning("tshark timed out after 5 minutes — killing process")
        Port.close(port)
        drain_port(port)
        {:ok, finalize(state)}
    end
  end

  defp finalize(state), do: %{state | entries: Enum.reverse(state.entries)}

  defp drain_port(port) do
    receive do
      {^port, _} -> drain_port(port)
    after
      0 -> :ok
    end
  end

  defp format_bytes(bytes) when bytes < 1024, do: "#{bytes} B"
  defp format_bytes(bytes) when bytes < 1024 * 1024, do: "#{Float.round(bytes / 1024, 1)} KB"
  defp format_bytes(bytes), do: "#{Float.round(bytes / (1024 * 1024), 1)} MB"

  defp log_result(result) do
    Logger.info(
      "Traffic extraction complete: #{result.count} entries, #{format_bytes(result.size)}"
    )
  end

  defp parse_line(line) do
    trimmed = String.trim(line)

    if trimmed == "" do
      []
    else
      case Jason.decode(trimmed) do
        {:ok, parsed} -> maybe_extract(parsed)
        {:error, _} -> []
      end
    end
  end

  defp maybe_extract(%{"index" => _}), do: []

  defp maybe_extract(%{"timestamp" => ts, "layers" => %{"http" => http} = layers}) do
    tcp = layers["tcp"]

    [
      %{
        timestamp: parse_timestamp(ts),
        src: {unwrap(layers["ip"]["ip_ip_src"]), parse_int(unwrap(tcp["tcp_tcp_srcport"]))},
        dst: {unwrap(layers["ip"]["ip_ip_dst"]), parse_int(unwrap(tcp["tcp_tcp_dstport"]))},
        method: unwrap(http["http_http_request_method"]),
        uri: unwrap(http["http_http_request_uri"]),
        status: parse_optional_int(unwrap(http["http_http_response_code"])),
        content_type: unwrap(http["http_http_content_type"]),
        body: decode_hex_body(unwrap(http["http_http_file_data"])),
        headers: extract_headers(http),
        stream_id: parse_optional_int(unwrap(tcp["tcp_tcp_stream"])),
        request_number: parse_optional_int(unwrap(http["http_http_request_number"])),
        response_number: parse_optional_int(unwrap(http["http_http_response_number"])),
        response_time: parse_optional_float(unwrap(http["http_http_time"]))
      }
    ]
  end

  defp maybe_extract(_), do: []

  defp parse_timestamp(millis_string) when is_binary(millis_string) do
    String.to_integer(millis_string) * 1_000
  end

  defp parse_int(s), do: String.to_integer(s)

  defp parse_optional_int(nil), do: nil
  defp parse_optional_int(s), do: String.to_integer(s)

  defp parse_optional_float(nil), do: nil
  defp parse_optional_float(s), do: String.to_float(s)

  defp extract_headers(http) do
    request_headers = List.wrap(http["http_http_request_line"])
    response_headers = List.wrap(http["http_http_response_line"])
    Enum.map(request_headers ++ response_headers, &String.trim/1)
  end

  # tshark -T ek wraps most field values in single-element arrays.
  defp unwrap([value]), do: value
  defp unwrap(value), do: value

  defp decode_hex_body(nil), do: nil

  defp decode_hex_body(hex) do
    # tshark uses colon-separated hex (e.g. "0b:1e:02"); strip colons first.
    plain = String.replace(hex, ":", "")
    padded = if rem(byte_size(plain), 2) == 1, do: plain <> "0", else: plain

    case Base.decode16(padded, case: :mixed) do
      {:ok, binary} -> binary
      :error -> nil
    end
  end
end
