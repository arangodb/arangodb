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

defmodule ToastTest.Formatting.BacktraceTest do
  use ExUnit.Case, async: true

  alias ToastTest.Formatting.Backtrace

  describe "format_backtrace/1" do
    test "numbers frames from zero and renders function with file:line" do
      frames = [
        %{function: "crash_func", file: "crash.cpp", line: 42},
        %{function: "worker_func", file: "worker.cpp", line: 10}
      ]

      assert Backtrace.format_backtrace(frames) ==
               "#0 crash_func at crash.cpp:42\n#1 worker_func at worker.cpp:10"
    end

    test "omits the location when file is missing" do
      assert Backtrace.format_backtrace([%{function: "naked", file: nil, line: nil}]) ==
               "#0 naked"
    end

    test "renders file without line when line is missing" do
      assert Backtrace.format_backtrace([%{function: "f", file: "x.cpp", line: nil}]) ==
               "#0 f at x.cpp"
    end

    test "empty frame list produces an empty string" do
      assert Backtrace.format_backtrace([]) == ""
    end
  end
end
