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

  # Cap stored body size to prevent ETF bloat on high-traffic suites.
  @max_body_bytes 8_192

  @type traffic_entry :: %{
          timestamp: integer(),
          src: {String.t(), integer()},
          dst: {String.t(), integer()},
          method: String.t() | nil,
          uri: String.t() | nil,
          status: integer() | nil,
          content_type: String.t() | nil,
          body: binary() | nil
        }

  @spec parse_tshark_output(String.t()) :: [traffic_entry()]
  def parse_tshark_output(json_lines) do
    json_lines
    |> String.split("\n")
    |> Enum.flat_map(&parse_line/1)
  end

  @spec run_tshark(String.t()) :: {:ok, String.t()} | {:error, term()}
  def run_tshark(pcap_path) do
    args = ["-r", pcap_path, "-T", "ek", "-Y", "http"]

    case System.find_executable("tshark") do
      nil ->
        {:error, :tshark_not_found}

      tshark ->
        case System.cmd(tshark, args, stderr_to_stdout: true) do
          {output, 0} -> {:ok, output}
          {output, code} -> {:error, {:tshark_failed, code, output}}
        end
    end
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
    [
      %{
        timestamp: parse_timestamp(ts),
        src: {layers["ip"]["ip_ip_src"], parse_int(layers["tcp"]["tcp_tcp_srcport"])},
        dst: {layers["ip"]["ip_ip_dst"], parse_int(layers["tcp"]["tcp_tcp_dstport"])},
        method: http["http_http_request_method"],
        uri: http["http_http_request_uri"],
        status: parse_optional_int(http["http_http_response_code"]),
        content_type: http["http_http_content_type"],
        body: decode_hex_body(http["http_http_file_data"])
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

  defp decode_hex_body(nil), do: nil

  defp decode_hex_body(hex) do
    padded = if rem(byte_size(hex), 2) == 1, do: hex <> "0", else: hex

    case Base.decode16(padded, case: :mixed) do
      {:ok, binary} ->
        if byte_size(binary) > @max_body_bytes do
          binary_part(binary, 0, @max_body_bytes)
        else
          binary
        end

      :error ->
        nil
    end
  end
end
