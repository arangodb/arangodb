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

defmodule Toast.SystemTest do
  use ExUnit.Case, async: true

  describe "parse_meminfo/1" do
    test "extracts MemTotal in bytes" do
      content = """
      MemTotal:       16384000 kB
      MemFree:         8192000 kB
      MemAvailable:   12000000 kB
      """

      assert Toast.System.parse_meminfo(content) == {:ok, 16_384_000 * 1024}
    end

    test "handles tabs and varying whitespace" do
      content = "MemTotal:\t\t 32768000 kB\nMemFree: 1000 kB\n"
      assert Toast.System.parse_meminfo(content) == {:ok, 32_768_000 * 1024}
    end

    test "returns :error when MemTotal line is missing" do
      content = "MemFree: 8192000 kB\n"
      assert Toast.System.parse_meminfo(content) == :error
    end

    test "returns :error for empty string" do
      assert Toast.System.parse_meminfo("") == :error
    end
  end

  describe "parse_cgroup_memory_max/1" do
    test "parses numeric limit" do
      assert Toast.System.parse_cgroup_memory_max("8589934592\n") == {:ok, 8_589_934_592}
    end

    test "returns :unlimited for 'max'" do
      assert Toast.System.parse_cgroup_memory_max("max\n") == :unlimited
    end

    test "returns :error for empty string" do
      assert Toast.System.parse_cgroup_memory_max("") == :error
    end

    test "returns :error for non-numeric content" do
      assert Toast.System.parse_cgroup_memory_max("garbage\n") == :error
    end

    test "strips whitespace" do
      assert Toast.System.parse_cgroup_memory_max("  4294967296  \n") == {:ok, 4_294_967_296}
    end
  end
end
