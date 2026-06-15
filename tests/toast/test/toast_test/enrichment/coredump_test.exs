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

defmodule ToastTest.Enrichment.CoredumpTest do
  use ExUnit.Case, async: true

  alias Toast.Diagnostics.Coredump.Report
  alias ToastTest.Enrichment.Coredump

  @default_executable "/usr/bin/arangod"

  describe "analyze/3" do
    test "transforms successful report into thread-list shape" do
      report = %Report{
        core_path: "/tmp/core.1234",
        binary_path: "/usr/bin/arangod",
        debugger: :gdb,
        signal: "SIGSEGV",
        faulting_address: "0xdeadbeef",
        registers: "rax  0x0  0\nrbx  0x7f  127",
        disassembly:
          "Dump of assembler code for function crash:\n=> mov eax,[rax]\nEnd of assembler dump.",
        crash_thread: 1,
        threads: [
          %{id: 1, frames: [%{function: "crash_func", file: "crash.cpp", line: 42}]},
          %{id: 2, frames: [%{function: "worker_func", file: "worker.cpp", line: 10}]}
        ]
      }

      result =
        Coredump.analyze("/tmp/core.1234", @default_executable,
          analyzer: fn _core, _bin, _opts -> {:ok, report} end
        )

      assert {:ok, enrichment} = result
      assert enrichment.signal == "SIGSEGV"
      assert enrichment.faulting_address == "0xdeadbeef"
      assert enrichment.registers =~ "rax"
      assert enrichment.disassembly =~ "Dump of assembler code"
      assert enrichment.debugger == :gdb
      assert enrichment.crash_thread == "1"
      assert length(enrichment.threads) == 2

      [thread1, thread2] = enrichment.threads
      assert thread1.id == "1"
      assert [%{function: "crash_func", file: "crash.cpp", line: 42}] = thread1.frames
      assert thread2.id == "2"
      assert [%{function: "worker_func", file: "worker.cpp", line: 10}] = thread2.frames
    end

    test "crash thread is reordered to first position" do
      report = %Report{
        core_path: "/tmp/core.1234",
        binary_path: "/usr/bin/arangod",
        debugger: :gdb,
        signal: "SIGSEGV",
        faulting_address: nil,
        crash_thread: 3,
        threads: [
          %{id: 1, frames: [%{function: "worker_a", file: "a.cpp", line: 1}]},
          %{id: 2, frames: [%{function: "worker_b", file: "b.cpp", line: 1}]},
          %{id: 3, frames: [%{function: "crash_func", file: "crash.cpp", line: 42}]}
        ]
      }

      assert {:ok, enrichment} =
               Coredump.analyze("/tmp/core.1234", @default_executable,
                 analyzer: fn _, _, _ -> {:ok, report} end
               )

      assert [first | rest] = enrichment.threads
      assert first.id == "3"
      assert [%{function: "crash_func"}] = first.frames
      rest_ids = Enum.map(rest, & &1.id)
      assert rest_ids == ["1", "2"]
    end

    test "thread order unchanged when crash_thread is nil" do
      report = %Report{
        core_path: "/tmp/core.1234",
        binary_path: "/usr/bin/arangod",
        debugger: :gdb,
        signal: nil,
        faulting_address: nil,
        crash_thread: nil,
        threads: [
          %{id: 1, frames: []},
          %{id: 2, frames: []},
          %{id: 3, frames: []}
        ]
      }

      assert {:ok, enrichment} =
               Coredump.analyze("/tmp/core.1234", @default_executable,
                 analyzer: fn _, _, _ -> {:ok, report} end
               )

      ids = Enum.map(enrichment.threads, & &1.id)
      assert ids == ["1", "2", "3"]
    end

    test "propagates analyzer errors" do
      assert {:error, :no_debugger} =
               Coredump.analyze("/tmp/core.1234", @default_executable,
                 analyzer: fn _, _, _ -> {:error, :no_debugger} end
               )
    end

    test "passes options through to analyzer" do
      test_pid = self()

      Coredump.analyze("/tmp/core.1234", @default_executable,
        timeout: 5_000,
        analyzer: fn core, bin, opts ->
          send(test_pid, {:called, core, bin, opts})
          {:error, :test}
        end
      )

      assert_received {:called, "/tmp/core.1234", "/usr/bin/arangod", opts}
      assert Keyword.get(opts, :timeout) == 5_000
    end
  end
end
