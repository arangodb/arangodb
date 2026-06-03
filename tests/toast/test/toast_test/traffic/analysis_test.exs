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

defmodule ToastTest.Traffic.AnalysisTest do
  use ExUnit.Case, async: true

  alias ToastTest.Traffic.Analysis

  # --- Test data helpers ---

  @server_ports %{8529 => "CRDN-abc", 8530 => "PRMR-xyz"}

  defp request(overrides \\ %{}) do
    Map.merge(
      %{
        timestamp: 1_000_000,
        src: {"10.0.0.1", 45678},
        dst: {"10.0.0.2", 8529},
        method: "GET",
        uri: "/_api/version",
        status: nil,
        content_type: "application/json",
        body: nil
      },
      overrides
    )
  end

  defp response(overrides \\ %{}) do
    Map.merge(
      %{
        timestamp: 1_000_100,
        src: {"10.0.0.2", 8529},
        dst: {"10.0.0.1", 45678},
        method: nil,
        uri: nil,
        status: 200,
        content_type: "application/json",
        body: ~s({"server":"arango"})
      },
      overrides
    )
  end

  # --- parse_method_filter/1 ---

  describe "parse_method_filter/1" do
    test "nil returns nil (no filter)" do
      assert Analysis.parse_method_filter(nil) == nil
    end

    test "single method" do
      assert Analysis.parse_method_filter("GET") == ["GET"]
    end

    test "multiple comma-separated methods" do
      assert Analysis.parse_method_filter("GET,POST") == ["GET", "POST"]
    end

    test "preserves case as given" do
      assert Analysis.parse_method_filter("get,Post") == ["get", "Post"]
    end
  end

  # --- parse_endpoint_filter/1 ---

  describe "parse_endpoint_filter/1" do
    test "nil returns nil (no filter)" do
      assert Analysis.parse_endpoint_filter(nil) == nil
    end

    test "single endpoint" do
      assert Analysis.parse_endpoint_filter("/_api/document") == ["/_api/document"]
    end

    test "multiple comma-separated endpoints" do
      result = Analysis.parse_endpoint_filter("/_api/document,/_api/cursor")
      assert result == ["/_api/document", "/_api/cursor"]
    end
  end

  # --- parse_status_filter/1 ---

  describe "parse_status_filter/1" do
    test "nil returns nil (no filter)" do
      assert Analysis.parse_status_filter(nil) == nil
    end

    test "single status becomes min == max range" do
      assert Analysis.parse_status_filter("200") == {200, 200}
    end

    test "range with dash" do
      assert Analysis.parse_status_filter("400-599") == {400, 599}
    end
  end

  # --- resolve_server/2 ---

  describe "resolve_server/2" do
    test "matches src port" do
      entry = response(%{src: {"10.0.0.2", 8529}})
      assert Analysis.resolve_server(entry, @server_ports) == "CRDN-abc"
    end

    test "matches dst port" do
      entry = request(%{dst: {"10.0.0.2", 8530}})
      assert Analysis.resolve_server(entry, @server_ports) == "PRMR-xyz"
    end

    test "returns nil when neither port matches" do
      entry = request(%{src: {"10.0.0.1", 9999}, dst: {"10.0.0.2", 9998}})
      assert Analysis.resolve_server(entry, @server_ports) == nil
    end
  end

  # --- annotate_server/2 ---

  describe "annotate_server/2" do
    test "request to server: dst port matches, direction is :request" do
      entry = request(%{dst: {"10.0.0.2", 8529}})
      assert Analysis.annotate_server(entry, @server_ports) == {"CRDN-abc", :request}
    end

    test "response from server: src port matches, direction is :response" do
      entry = response(%{src: {"10.0.0.2", 8529}})
      assert Analysis.annotate_server(entry, @server_ports) == {"CRDN-abc", :response}
    end

    test "neither port matches: nil server and :unknown direction" do
      entry = request(%{src: {"10.0.0.1", 9999}, dst: {"10.0.0.2", 9998}})
      assert Analysis.annotate_server(entry, @server_ports) == {nil, :unknown}
    end
  end

  # --- format_entry/2 ---

  describe "format_entry/1" do
    test "formats a request with method and URI" do
      entry = request(%{method: "GET", uri: "/_api/version"})
      result = Analysis.format_entry(entry)
      assert result =~ ~r/→.*GET \/\_api\/version/u
    end

    test "formats a response with status, content type, and body size" do
      body = ~s({"server":"arango"})
      entry = response(%{status: 200, content_type: "application/json", body: body})
      result = Analysis.format_entry(entry)
      assert result =~ ~r/←.*200/u
      assert result =~ "application/json"
      assert result =~ "#{byte_size(body)} bytes"
    end

    test "formats a response with nil body without size" do
      entry = response(%{body: nil})
      result = Analysis.format_entry(entry)
      assert result =~ ~r/←.*200/u
      refute result =~ "bytes"
    end

    test "notes VPack for velocypack content type" do
      entry = response(%{content_type: "application/x-velocypack", body: <<0x01, 0x02>>})
      result = Analysis.format_entry(entry)
      assert result =~ "VPack" or result =~ "velocypack"
    end

    test "formats a POST request" do
      entry = request(%{method: "POST", uri: "/_api/document/col"})
      result = Analysis.format_entry(entry)
      assert result =~ ~r/→.*POST \/\_api\/document\/col/u
    end
  end

  # --- extract/4: time window ---

  describe "extract/4 time window filtering" do
    test "includes entries within the window" do
      entries = [request(%{timestamp: 500}), request(%{timestamp: 1000})]
      result = Analysis.extract(entries, @server_ports, {400, 1100}, [])
      assert length(result) == 2
    end

    test "excludes entries outside the window" do
      entries = [
        request(%{timestamp: 100}),
        request(%{timestamp: 500}),
        request(%{timestamp: 2000})
      ]

      result = Analysis.extract(entries, @server_ports, {400, 1000}, [])
      assert length(result) == 1
      assert hd(result).timestamp == 500
    end

    test "includes entries exactly at window boundaries" do
      entries = [request(%{timestamp: 400}), request(%{timestamp: 1000})]
      result = Analysis.extract(entries, @server_ports, {400, 1000}, [])
      assert length(result) == 2
    end

    test "returns empty list when no entries fall in window" do
      entries = [request(%{timestamp: 100}), request(%{timestamp: 200})]
      result = Analysis.extract(entries, @server_ports, {400, 1000}, [])
      assert result == []
    end
  end

  # --- extract/4: server filter ---

  describe "extract/4 server filter" do
    test "without server_filter option, includes all entries with matching ports" do
      entries = [
        request(%{timestamp: 500, dst: {"10.0.0.2", 8529}}),
        request(%{timestamp: 600, dst: {"10.0.0.2", 9999}})
      ]

      result = Analysis.extract(entries, @server_ports, {0, 10_000}, [])
      # Both should be included when no server_filter is given:
      # entry with port 8529 matches, entry with port 9999 has no matching server.
      # The spec says server_ports is used for filtering only when server_filter is in opts.
      assert length(result) == 2
    end

    test "with server_filter, only includes entries involving matching servers" do
      entries = [
        request(%{timestamp: 500, dst: {"10.0.0.2", 8529}}),
        request(%{timestamp: 600, dst: {"10.0.0.2", 8530}}),
        request(%{timestamp: 700, src: {"10.0.0.1", 9999}, dst: {"10.0.0.2", 9998}})
      ]

      # Filter to only CRDN-abc (port 8529)
      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, server_filter: ["CRDN-abc"])

      assert length(result) == 1
      assert hd(result).timestamp == 500
    end

    test "entries where src port matches server are also included by server_filter" do
      entries = [response(%{timestamp: 500, src: {"10.0.0.2", 8530}})]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, server_filter: ["PRMR-xyz"])

      assert length(result) == 1
    end
  end

  # --- extract/4: method filter ---

  describe "extract/4 method filter" do
    test "filters requests by method" do
      entries = [
        request(%{timestamp: 500, method: "GET"}),
        request(%{timestamp: 600, method: "POST"}),
        request(%{timestamp: 700, method: "DELETE"})
      ]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, method_filter: ["GET", "POST"])

      assert length(result) == 2
      methods = Enum.map(result, & &1.method)
      assert "GET" in methods
      assert "POST" in methods
    end

    test "responses always pass method filter (nil method)" do
      entries = [
        request(%{timestamp: 500, method: "DELETE"}),
        response(%{timestamp: 600})
      ]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, method_filter: ["GET"])

      # DELETE request filtered out, but response passes through
      assert length(result) == 1
      assert hd(result).status == 200
    end

    test "no method_filter includes all methods" do
      entries = [
        request(%{timestamp: 500, method: "GET"}),
        request(%{timestamp: 600, method: "POST"})
      ]

      result = Analysis.extract(entries, @server_ports, {0, 10_000}, [])
      assert length(result) == 2
    end
  end

  # --- extract/4: endpoint filter ---

  describe "extract/4 endpoint filter" do
    test "filters requests by URI substring" do
      entries = [
        request(%{timestamp: 500, uri: "/_api/document/users"}),
        request(%{timestamp: 600, uri: "/_api/cursor"}),
        request(%{timestamp: 700, uri: "/_api/version"})
      ]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000},
          endpoint_filter: ["/_api/document", "/_api/cursor"]
        )

      assert length(result) == 2
      uris = Enum.map(result, & &1.uri)
      assert "/_api/document/users" in uris
      assert "/_api/cursor" in uris
    end

    test "responses always pass endpoint filter (nil URI)" do
      entries = [
        request(%{timestamp: 500, uri: "/_api/version"}),
        response(%{timestamp: 600})
      ]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, endpoint_filter: ["/_api/document"])

      assert length(result) == 1
      assert hd(result).status == 200
    end

    test "substring matching: filter matches partial URI" do
      entries = [request(%{timestamp: 500, uri: "/_api/document/my_collection/12345"})]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, endpoint_filter: ["/_api/document"])

      assert length(result) == 1
    end
  end

  # --- extract/4: status filter ---

  describe "extract/4 status filter" do
    test "filters responses by status range" do
      entries = [
        response(%{timestamp: 500, status: 200}),
        response(%{timestamp: 600, status: 404}),
        response(%{timestamp: 700, status: 500})
      ]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, status_filter: {400, 599})

      assert length(result) == 2
      statuses = Enum.map(result, & &1.status)
      assert 404 in statuses
      assert 500 in statuses
    end

    test "requests always pass status filter (nil status)" do
      entries = [
        request(%{timestamp: 500}),
        response(%{timestamp: 600, status: 200})
      ]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, status_filter: {400, 599})

      # Request passes (nil status), response 200 filtered out
      assert length(result) == 1
      assert hd(result).method == "GET"
    end

    test "status at range boundaries is included" do
      entries = [
        response(%{timestamp: 500, status: 400}),
        response(%{timestamp: 600, status: 599})
      ]

      result =
        Analysis.extract(entries, @server_ports, {0, 10_000}, status_filter: {400, 599})

      assert length(result) == 2
    end
  end

  # --- extract/4: combined filters ---

  describe "extract/4 combined filters" do
    test "all filters are applied together" do
      entries = [
        # In window, POST to /_api/document, passes all
        request(%{
          timestamp: 500,
          method: "POST",
          uri: "/_api/document/col",
          dst: {"10.0.0.2", 8529}
        }),
        # In window, GET (wrong method for method_filter)
        request(%{
          timestamp: 600,
          method: "GET",
          uri: "/_api/document/col",
          dst: {"10.0.0.2", 8529}
        }),
        # In window, POST but wrong endpoint
        request(%{timestamp: 700, method: "POST", uri: "/_api/version", dst: {"10.0.0.2", 8529}}),
        # Outside window
        request(%{
          timestamp: 5000,
          method: "POST",
          uri: "/_api/document/col",
          dst: {"10.0.0.2", 8529}
        }),
        # Response in window, error status passes status filter
        response(%{timestamp: 550, status: 409, src: {"10.0.0.2", 8529}})
      ]

      result =
        Analysis.extract(entries, @server_ports, {0, 1000},
          method_filter: ["POST"],
          endpoint_filter: ["/_api/document"],
          status_filter: {400, 599}
        )

      assert length(result) == 2
      timestamps = Enum.map(result, & &1.timestamp)
      assert 500 in timestamps
      assert 550 in timestamps
    end
  end

  # --- extract/4: ordering ---

  describe "extract/4 result ordering" do
    test "results are sorted by timestamp" do
      entries = [
        request(%{timestamp: 900}),
        request(%{timestamp: 300}),
        response(%{timestamp: 600}),
        request(%{timestamp: 100})
      ]

      result = Analysis.extract(entries, @server_ports, {0, 10_000}, [])
      timestamps = Enum.map(result, & &1.timestamp)
      assert timestamps == [100, 300, 600, 900]
    end

    test "empty input returns empty list" do
      assert Analysis.extract([], @server_ports, {0, 10_000}, []) == []
    end
  end
end
