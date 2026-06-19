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
      |> maybe_put("http_http_request_number", params[:request_number])
      |> maybe_put("http_http_response_number", params[:response_number])
      |> maybe_put("http_http_time", params[:response_time])
      |> maybe_put("http_http_request_line", params[:request_headers])
      |> maybe_put("http_http_response_line", params[:response_headers])

    tcp_fields =
      %{"tcp_tcp_srcport" => params.src_port, "tcp_tcp_dstport" => params.dst_port}
      |> maybe_put("tcp_tcp_stream", params[:stream_id])

    data = %{
      "timestamp" => params.timestamp,
      "layers" => %{
        "ip" => %{"ip_ip_src" => params.src_ip, "ip_ip_dst" => params.dst_ip},
        "tcp" => tcp_fields,
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

    test "decodes colon-separated hex body (tshark format)" do
      colon_hex = "7b:22:73:65:72:76:65:72:22:3a:22:61:72:61:6e:67:6f:22:7d"
      input = response_line(%{body_hex: colon_hex})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.body == ~s({"server":"arango"})
    end

    test "decodes colon-separated VPack hex body" do
      colon_hex = "01:02:03:04:05:ff"
      input = response_line(%{body_hex: colon_hex, content_type: "application/x-velocypack"})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.body == <<0x01, 0x02, 0x03, 0x04, 0x05, 0xFF>>
    end

    test "pads odd-length hex body before decoding" do
      input = response_line(%{body_hex: "4f4b0", content_type: "application/octet-stream"})
      [entry] = Extraction.parse_tshark_output(input)

      assert entry.body == <<0x4F, 0x4B, 0x00>>
    end
  end

  describe "new fields (headers, stream_id, pairing, response_time)" do
    test "extracts request headers" do
      input =
        request_line(%{
          request_headers: ["content-type: application/json\r\n", "host: localhost\r\n"]
        })

      [entry] = Extraction.parse_tshark_output(input)
      assert entry.headers == ["content-type: application/json", "host: localhost"]
    end

    test "extracts response headers" do
      input =
        response_line(%{
          response_headers: ["Content-Type: application/json\r\n", "X-Arango-Foo: bar\r\n"]
        })

      [entry] = Extraction.parse_tshark_output(input)
      assert entry.headers == ["Content-Type: application/json", "X-Arango-Foo: bar"]
    end

    test "headers default to empty list when absent" do
      [entry] = Extraction.parse_tshark_output(request_line())
      assert entry.headers == []
    end

    test "extracts stream_id and request_number" do
      input = request_line(%{stream_id: "12", request_number: "3"})
      [entry] = Extraction.parse_tshark_output(input)
      assert entry.stream_id == 12
      assert entry.request_number == 3
      assert entry.response_number == nil
    end

    test "extracts stream_id and response_number" do
      input =
        response_line(%{stream_id: "12", response_number: "3", response_time: "0.000698000"})

      [entry] = Extraction.parse_tshark_output(input)
      assert entry.stream_id == 12
      assert entry.response_number == 3
      assert entry.request_number == nil
    end

    test "extracts response_time as float" do
      input = response_line(%{response_time: "0.000698000"})
      [entry] = Extraction.parse_tshark_output(input)
      assert_in_delta entry.response_time, 0.000698, 0.0000001
    end

    test "response_time is nil when absent" do
      [entry] = Extraction.parse_tshark_output(request_line())
      assert entry.response_time == nil
    end

    test "stream_id is nil when absent" do
      [entry] = Extraction.parse_tshark_output(request_line())
      assert entry.stream_id == nil
    end
  end

  @pcap_fixture Path.expand("../../fixtures/traffic/sample.pcap", __DIR__)

  describe "extract/1 integration" do
    @tag :integration
    test "extracts entries with bodies from a real pcap" do
      assert {:ok, entries} = Extraction.extract(@pcap_fixture)
      assert length(entries) > 0

      requests = Enum.filter(entries, & &1.method)
      responses = Enum.filter(entries, & &1.status)
      assert requests != []
      assert responses != []

      with_body = Enum.filter(entries, & &1.body)
      assert with_body != [], "expected at least some entries to have bodies"

      Enum.each(with_body, fn entry ->
        assert is_binary(entry.body)
        assert byte_size(entry.body) > 0
      end)
    end

    @tag :integration
    test "returns an error when tshark fails to process the file" do
      bad = Path.join(System.tmp_dir!(), "toast_invalid.pcap")
      File.write!(bad, "not a pcap file")
      on_exit(fn -> File.rm(bad) end)

      assert {:error, {:tshark_exited, code}} = Extraction.extract(bad)
      assert code != 0
    end

    @tag :integration
    test "VPack bodies are decodable" do
      assert {:ok, entries} = Extraction.extract(@pcap_fixture)

      vpack_entries =
        entries
        |> Enum.filter(
          &(&1.body && &1.content_type && String.contains?(&1.content_type, "velocypack"))
        )

      assert vpack_entries != [], "expected VPack entries in sample pcap"

      Enum.each(vpack_entries, fn entry ->
        assert {:ok, decoded} = VelocyPack.decode(entry.body),
               "VPack body should decode successfully"

        assert decoded != nil
      end)
    end
  end
end
