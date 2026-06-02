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

defmodule Toast.Deployment.NetstatTest do
  use ExUnit.Case, async: true

  alias Toast.Deployment.Netstat

  @ss_output """
  ESTAB      0       0      127.0.0.1:8530         127.0.0.1:45678  users:(("arangod",pid=1234,fd=10))
  ESTAB      0       0      127.0.0.1:45678        127.0.0.1:8530   users:(("arangod",pid=1234,fd=11))
  ESTAB      0       0      127.0.0.1:8531         127.0.0.1:50000  users:(("arangod",pid=5678,fd=20))
  CLOSE-WAIT 0       0      127.0.0.1:8531         127.0.0.1:50001  users:(("arangod",pid=5678,fd=21))
  ESTAB      0       0      127.0.0.1:9999         127.0.0.1:12345  users:(("other",pid=9999,fd=5))
  """

  @netstat_output """
  tcp        0      0 127.0.0.1:8530          127.0.0.1:45678         ESTABLISHED 1234/arangod
  tcp        0      0 127.0.0.1:45678         127.0.0.1:8530          ESTABLISHED 1234/arangod
  tcp        0      0 127.0.0.1:8531          127.0.0.1:50000         ESTABLISHED 5678/arangod
  tcp        0      0 127.0.0.1:8531          127.0.0.1:50001         CLOSE_WAIT  5678/arangod
  tcp        0      0 127.0.0.1:9999          127.0.0.1:12345         ESTABLISHED 9999/other
  """

  @servers [
    %{id: "single-0", pid: 1234, port: 8530},
    %{id: "dbserver-0", pid: 5678, port: 8531}
  ]

  describe "count_lines/1" do
    test "counts non-empty lines" do
      assert Netstat.count_lines("line1\nline2\nline3\n") == 3
    end

    test "returns 0 for empty output" do
      assert Netstat.count_lines("") == 0
    end

    test "handles output without trailing newline" do
      assert Netstat.count_lines("line1\nline2") == 2
    end
  end

  describe "build_snapshot_by_pid/3 with ss" do
    test "groups sockets by server PID with in/out classification" do
      snapshot = Netstat.build_snapshot_by_pid(@ss_output, @servers, :ss)

      assert %{"single-0" => single, "dbserver-0" => dbserver} = snapshot

      assert single.pid == 1234
      assert single.sockets.total == 2
      assert single.sockets.in == %{"ESTAB" => 1}
      assert single.sockets.out == %{"ESTAB" => 1}

      assert dbserver.pid == 5678
      assert dbserver.sockets.total == 2
      assert dbserver.sockets.in == %{"ESTAB" => 1, "CLOSE-WAIT" => 1}
      assert dbserver.sockets.out == %{}
    end

    test "includes servers with zero sockets" do
      snapshot = Netstat.build_snapshot_by_pid("", @servers, :ss)

      assert %{"single-0" => single, "dbserver-0" => dbserver} = snapshot
      assert single.sockets.total == 0
      assert dbserver.sockets.total == 0
    end

    test "ignores sockets belonging to unknown PIDs" do
      output = """
      ESTAB      0       0      127.0.0.1:9999         127.0.0.1:12345  users:(("other",pid=9999,fd=5))
      """

      snapshot = Netstat.build_snapshot_by_pid(output, @servers, :ss)

      assert snapshot["single-0"].sockets.total == 0
      assert snapshot["dbserver-0"].sockets.total == 0
    end
  end

  describe "build_snapshot_by_pid/3 with netstat" do
    test "groups sockets by server PID with in/out classification" do
      snapshot = Netstat.build_snapshot_by_pid(@netstat_output, @servers, :netstat)

      assert %{"single-0" => single, "dbserver-0" => dbserver} = snapshot

      assert single.pid == 1234
      assert single.sockets.total == 2
      assert single.sockets.in == %{"ESTABLISHED" => 1}
      assert single.sockets.out == %{"ESTABLISHED" => 1}

      assert dbserver.pid == 5678
      assert dbserver.sockets.total == 2
      assert dbserver.sockets.in == %{"ESTABLISHED" => 1, "CLOSE_WAIT" => 1}
      assert dbserver.sockets.out == %{}
    end

    test "ignores sockets belonging to unknown PIDs" do
      output = """
      tcp        0      0 127.0.0.1:9999          127.0.0.1:12345         ESTABLISHED 9999/other
      """

      snapshot = Netstat.build_snapshot_by_pid(output, @servers, :netstat)

      assert snapshot["single-0"].sockets.total == 0
      assert snapshot["dbserver-0"].sockets.total == 0
    end
  end
end
