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

defmodule ToastTest.DebuggerAttachTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.ServerInstance
  alias ToastTest.DebuggerAttach

  defp server(id, endpoint, pid) do
    %ServerInstance{id: id, role: :single, endpoint: endpoint, pid: pid}
  end

  describe "format_server_table/1" do
    test "formats single server with pid and endpoint" do
      servers = [server("single", "http://127.0.0.1:8529", 12_345)]

      assert DebuggerAttach.format_server_table(servers) == [
               "  single  pid=12345  http://127.0.0.1:8529"
             ]
    end

    test "pads server IDs to align columns" do
      servers = [
        server("dbserver-0", "http://127.0.0.1:8530", 100),
        server("coordinator-0", "http://127.0.0.1:8529", 200)
      ]

      lines = DebuggerAttach.format_server_table(servers)

      assert lines == [
               "  dbserver-0     pid=100  http://127.0.0.1:8530",
               "  coordinator-0  pid=200  http://127.0.0.1:8529"
             ]
    end

    test "handles missing pid" do
      servers = [server("single", "http://127.0.0.1:8529", nil)]

      assert DebuggerAttach.format_server_table(servers) == [
               "  single  pid=?  http://127.0.0.1:8529"
             ]
    end
  end

  describe "format_attach_commands/2" do
    test "formats attach commands with server id comment" do
      servers = [
        server("dbserver-0", "http://127.0.0.1:8530", 100),
        server("coordinator-0", "http://127.0.0.1:8529", 200)
      ]

      lines = DebuggerAttach.format_attach_commands(servers, "/usr/bin/gdb")

      assert lines == [
               "  /usr/bin/gdb -p 100   # dbserver-0",
               "  /usr/bin/gdb -p 200   # coordinator-0"
             ]
    end

    test "skips servers without pid" do
      servers = [
        server("single", "http://127.0.0.1:8529", nil),
        server("other", "http://127.0.0.1:8530", 999)
      ]

      lines = DebuggerAttach.format_attach_commands(servers, "lldb")

      assert lines == ["  lldb -p 999   # other"]
    end
  end
end
