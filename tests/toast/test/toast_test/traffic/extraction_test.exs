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

defmodule ToastTest.Traffic.ExtractionTest do
  use ExUnit.Case, async: true

  alias ToastTest.Traffic.Extraction

  # --- Test data helpers ---

  defp request_line(overrides \\ %{}) do
    defaults = %{
      timestamp: "1717300000123",
      src_ip: "10.0.0.1",
      dst_ip: "10.0.0.2",
      src_port: "45678",
      dst_port: "8529",
      method: "GET",
      uri: "/_api/version",
      content_type: "application/json",
      body_hex: nil
    }

    build_data_line(Map.merge(defaults, overrides))
  end

  defp response_line(overrides \\ %{}) do
    defaults = %{
      timestamp: "1717300000234",
      src_ip: "10.0.0.2",
      dst_ip: "10.0.0.1",
      src_port: "8529",
      dst_port: "45678",
      status: "200",
      content_type: "application/json",
      body_hex: "7b22736572766572223a226172616e676f227d"
    }

    build_data_line(Map.merge(defaults, overrides))
  end

  defp build_data_line(params) do
    http_fields =
      %{}
      |> maybe_put("http_http_request_method", params[:method])
      |> maybe_put("http_http_request_uri", params[:uri])
      |> maybe_put("http_http_response_code", params[:status])
      |> maybe_put("http_http_content_type", params[:content_type])
      |> maybe_put("http_http_file_data", params[:body_hex])

    data = %{
      "timestamp" => params.timestamp,
      "layers" => %{
        "ip" => %{"ip_ip_src" => params.src_ip, "ip_ip_dst" => params.dst_ip},
        "tcp" => %{"tcp_tcp_srcport" => params.src_port, "tcp_tcp_dstport" => params.dst_port},
        "http" => http_fields
      }
    }

    Jason.encode!(data)
  end

  defp maybe_put(map, _key, nil), do: map
  defp maybe_put(map, key, value), do: Map.put(map, key, value)

  defp index_line do
    ~s({"index":{"_index":"packets-2024-06-02","_type":"doc"}})
  end

  # --- Tests ---

  describe "parse_tshark_output/1" do
    test "parses an HTTP request with all fields" do
      input = request_line()
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.timestamp == 1_717_300_000_123_000
      assert entry.src == {"10.0.0.1", 45678}
      assert entry.dst == {"10.0.0.2", 8529}
      assert entry.method == "GET"
      assert entry.uri == "/_api/version"
      assert entry.status == nil
      assert entry.content_type == "application/json"
      assert entry.body == nil
    end

    test "parses an HTTP response with hex-decoded body" do
      input = response_line()
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.timestamp == 1_717_300_000_234_000
      assert entry.src == {"10.0.0.2", 8529}
      assert entry.dst == {"10.0.0.1", 45678}
      assert entry.method == nil
      assert entry.uri == nil
      assert entry.status == 200
      assert entry.content_type == "application/json"
      assert entry.body == ~s({"server":"arango"})
    end

    test "skips index lines" do
      input =
        [index_line(), request_line(), index_line(), response_line(), index_line()]
        |> Enum.join("\n")

      entries = Extraction.parse_tshark_output(input)
      assert length(entries) == 2
      assert Enum.at(entries, 0).method == "GET"
      assert Enum.at(entries, 1).status == 200
    end

    test "returns empty list for empty input" do
      assert Extraction.parse_tshark_output("") == []
    end

    test "returns empty list for whitespace-only input" do
      assert Extraction.parse_tshark_output("  \n  \n  ") == []
    end

    test "handles request with missing content_type and body" do
      input = request_line(%{content_type: nil, body_hex: nil})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.method == "GET"
      assert entry.uri == "/_api/version"
      assert entry.content_type == nil
      assert entry.body == nil
    end

    test "handles response with missing body" do
      input = response_line(%{body_hex: nil})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.status == 200
      assert entry.body == nil
    end

    test "handles response with missing content_type" do
      input = response_line(%{content_type: nil})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.status == 200
      assert entry.content_type == nil
    end

    test "decodes empty hex body as empty binary" do
      input = response_line(%{body_hex: ""})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.body == ""
    end

    test "decodes hex body with non-UTF8 content (VPack)" do
      # VPack-like binary data that is not valid UTF-8
      vpack_hex = "0102030405ff"
      input = response_line(%{body_hex: vpack_hex, content_type: "application/x-velocypack"})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.body == <<0x01, 0x02, 0x03, 0x04, 0x05, 0xFF>>
      assert entry.content_type == "application/x-velocypack"
    end

    test "preserves order of multiple entries" do
      lines = [
        request_line(%{
          timestamp: "1717300001000",
          method: "POST",
          uri: "/_api/document/col"
        }),
        response_line(%{timestamp: "1717300002000", status: "201"}),
        request_line(%{
          timestamp: "1717300003000",
          method: "DELETE",
          uri: "/_api/document/col/123"
        })
      ]

      entries = Extraction.parse_tshark_output(Enum.join(lines, "\n"))

      assert length(entries) == 3
      assert Enum.at(entries, 0).method == "POST"
      assert Enum.at(entries, 0).uri == "/_api/document/col"
      assert Enum.at(entries, 1).status == 201
      assert Enum.at(entries, 2).method == "DELETE"
      assert Enum.at(entries, 2).uri == "/_api/document/col/123"
    end

    test "skips malformed JSON lines gracefully" do
      lines = [
        request_line(),
        "this is not json at all",
        "{broken json",
        response_line()
      ]

      entries = Extraction.parse_tshark_output(Enum.join(lines, "\n"))

      assert length(entries) == 2
      assert Enum.at(entries, 0).method == "GET"
      assert Enum.at(entries, 1).status == 200
    end

    test "converts millisecond-epoch timestamp to microseconds" do
      input = request_line(%{timestamp: "1717300000001"})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.timestamp == 1_717_300_000_001_000
    end

    test "converts round-second timestamp to microseconds" do
      input = request_line(%{timestamp: "1717300000000"})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.timestamp == 1_717_300_000_000_000
    end

    test "parses POST request with body" do
      body = ~s({"name":"test"})
      body_hex = Base.encode16(body, case: :lower)

      input =
        request_line(%{
          method: "POST",
          uri: "/_api/document/users",
          body_hex: body_hex
        })

      [entry] = Extraction.parse_tshark_output(input)

      assert entry.method == "POST"
      assert entry.uri == "/_api/document/users"
      assert entry.body == body
    end

    test "handles lines with trailing newlines" do
      input = request_line() <> "\n\n\n"
      entries = Extraction.parse_tshark_output(input)

      assert length(entries) == 1
    end

    test "parses ports as integers in src/dst tuples" do
      input = request_line(%{src_port: "12345", dst_port: "8529"})
      [entry] = Extraction.parse_tshark_output(input)

      assert {_, src_port} = entry.src
      assert {_, dst_port} = entry.dst
      assert is_integer(src_port)
      assert is_integer(dst_port)
      assert src_port == 12345
      assert dst_port == 8529
    end
  end
end
