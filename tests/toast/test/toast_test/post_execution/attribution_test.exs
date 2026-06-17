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

defmodule ToastTest.PostExecution.AttributionTest do
  @moduledoc """
  `Attribution.run/2` is pure: it decides issues over an already-enriched
  `Attribution.Inputs` bundle (failures, enriched crash events, parsed sanitizer
  reports, timeout kills, infrastructure events) within a temporal frame
  (`windows`) — no file I/O. The enrichment that turns paths into that data is
  exercised in `ToastTest.PostExecution.EnrichmentTest`. The coredump-report aggregate is no
  longer produced here — PostExecution projects it from the enriched crashes.
  """
  use ExUnit.Case, async: true

  import ToastTest.TimeTestHelpers, only: [to_us: 1]

  alias ToastTest.CrashEvent
  alias Toast.Process.CrashInfo
  alias ToastTest.PostExecution.Attribution
  alias ToastTest.PostExecution.Attribution.Inputs

  # Suite: 10:00:00 - 10:10:00
  # ModA:  10:00:01 - 10:05:00
  #   test_one: 10:00:03 - 10:01:00
  #   test_two: 10:01:05 - 10:02:00

  @mod_a_started ~U[2026-03-09 10:00:01Z]
  @mod_a_finished ~U[2026-03-09 10:05:00Z]

  @test1_started ~U[2026-03-09 10:00:03Z]
  @test1_finished ~U[2026-03-09 10:01:00Z]
  @test2_started ~U[2026-03-09 10:01:05Z]
  @test2_finished ~U[2026-03-09 10:02:00Z]

  @default_executable "/usr/bin/arangod"

  # Mirrors the output of TimeWindows.build/1 (kept decoupled so these tests
  # exercise attribution logic, not window construction). Keep in sync if the
  # window shape changes.
  defp build_windows do
    %{
      modules: %{
        ModA => %{
          started_at: to_us(@mod_a_started),
          finished_at: to_us(@mod_a_finished),
          setup_finished_at: to_us(@test1_started),
          teardown_started_at: to_us(@test2_finished)
        }
      },
      tests: %{
        {ModA, :test_one} => %{
          started_at: to_us(@test1_started),
          finished_at: to_us(@test1_finished)
        },
        {ModA, :test_two} => %{
          started_at: to_us(@test2_started),
          finished_at: to_us(@test2_finished)
        }
      }
    }
  end

  # An enriched crash event as produced by ToastTest.PostExecution.Enrichment: `effective_at`
  # set, coredump reports already analyzed.
  defp enriched_crash(server_id, effective_at, opts \\ []) do
    %CrashEvent{
      server_id: server_id,
      crash_info: %CrashInfo{
        exit_status: 139,
        signal: 11,
        executable: @default_executable,
        timestamp: effective_at,
        os_pid: Keyword.get(opts, :os_pid)
      },
      effective_at: effective_at,
      coredump_reports: Keyword.get(opts, :coredump_reports, []),
      log_file: Keyword.get(opts, :log_file),
      crash_lines: Keyword.get(opts, :crash_lines)
    }
  end

  defp coredump_report(core_path, server_id) do
    %{
      core_path: core_path,
      server_id: server_id,
      debugger: :gdb,
      signal: "SIGSEGV",
      faulting_address: nil,
      registers: nil,
      disassembly: nil,
      crash_thread: "1",
      threads: [%{id: "1", frames: [%{function: "boom", file: "x.cpp", line: 1}]}]
    }
  end

  defp make_exunit_test(module, name, state) do
    %ExUnit.Test{
      name: name,
      module: module,
      state: state,
      tags: %{file: "test.exs", line: 1}
    }
  end

  # --- No issues ---

  describe "run/2 — empty inputs" do
    test "no failures, no crashes, no reports returns []" do
      assert [] = Attribution.run(%Inputs{}, build_windows())
    end
  end

  # --- Test failures ---

  describe "run/2 — test failures" do
    test "each failure becomes a :test_failure issue" do
      failure1 = make_exunit_test(ModA, :test_one, {:failed, [{:error, %{message: "boom"}, []}]})
      failure2 = make_exunit_test(ModA, :test_two, {:failed, [{:error, %{message: "bang"}, []}]})

      issues = Attribution.run(%Inputs{failures: [failure1, failure2]}, build_windows())

      assert length(issues) == 2

      assert Enum.all?(issues, &(&1.type == :test_failure))
      assert Enum.all?(issues, &(&1.confidence == nil))

      scopes = Enum.map(issues, & &1.scope)
      assert {:test, ModA, :test_one} in scopes
      assert {:test, ModA, :test_two} in scopes
    end

    test "failure detail contains the ExUnit.Test struct" do
      failure = make_exunit_test(ModA, :test_one, {:failed, [{:error, %{message: "boom"}, []}]})

      assert [issue] = Attribution.run(%Inputs{failures: [failure]}, build_windows())

      assert issue.detail.test == failure
    end
  end

  # --- Crashes ---

  describe "run/2 — crash events" do
    test "crash attributed to a test window via effective_at" do
      crashes = [enriched_crash("single1", to_us(~U[2026-03-09 10:00:30Z]))]

      assert [issue] = Attribution.run(%Inputs{crashes: crashes}, build_windows())
      assert issue.type == :crash
      assert issue.scope == {:test, ModA, :test_one}
      assert issue.confidence == :high
      assert issue.detail.server == "single1"
      assert issue.detail.effective_at == to_us(~U[2026-03-09 10:00:30Z])
    end

    test "crash without a matching window falls back to :suite" do
      crashes = [enriched_crash("single1", to_us(~U[2026-03-09 10:08:00Z]))]

      assert [issue] = Attribution.run(%Inputs{crashes: crashes}, build_windows())
      assert issue.scope == :suite
    end

    test "raw crash_info.timestamp is preserved as provenance, distinct from effective_at" do
      detected = to_us(~U[2026-03-09 10:00:31Z])
      effective = to_us(~U[2026-03-09 10:00:30Z])

      crash = %{
        enriched_crash("single1", effective)
        | crash_info: %CrashInfo{
            exit_status: 139,
            signal: 11,
            executable: @default_executable,
            timestamp: detected
          }
      }

      assert [issue] = Attribution.run(%Inputs{crashes: [crash]}, build_windows())

      assert issue.detail.effective_at == effective
      assert issue.detail.crash_info.timestamp == detected
    end

    test "enriched coredump reports become issue coredump_paths" do
      crashes = [
        enriched_crash("single1", to_us(~U[2026-03-09 10:00:30Z]),
          coredump_reports: [coredump_report("/tmp/core.1234", "single1")]
        )
      ]

      assert [issue] = Attribution.run(%Inputs{crashes: crashes}, build_windows())
      assert issue.detail.coredump_paths == ["/tmp/core.1234"]
    end

    test "multiple coredump reports map to multiple paths" do
      crashes = [
        enriched_crash("single1", to_us(~U[2026-03-09 10:00:30Z]),
          coredump_reports: [
            coredump_report("/tmp/core.1", "single1"),
            coredump_report("/tmp/core.2", "single1")
          ]
        )
      ]

      assert [issue] = Attribution.run(%Inputs{crashes: crashes}, build_windows())
      assert Enum.sort(issue.detail.coredump_paths) == ["/tmp/core.1", "/tmp/core.2"]
    end

    test "a crash with no coredump reports omits coredump_paths" do
      crashes = [enriched_crash("single1", to_us(~U[2026-03-09 10:00:30Z]))]

      assert [issue] = Attribution.run(%Inputs{crashes: crashes}, build_windows())
      assert issue.type == :crash
      refute Map.has_key?(issue.detail, :coredump_paths)
    end

    test "crash log lines and log_file flow into the issue detail" do
      crashes = [
        enriched_crash("single1", to_us(~U[2026-03-09 10:00:30Z]),
          crash_lines: "[FATAL] {crash} boom",
          log_file: "/tmp/single1/arangod.log"
        )
      ]

      assert [issue] = Attribution.run(%Inputs{crashes: crashes}, build_windows())

      assert issue.detail.crash_lines == "[FATAL] {crash} boom"
      assert issue.detail.log_file == "/tmp/single1/arangod.log"
    end
  end

  # --- Sanitizer reports ---

  describe "run/2 — sanitizer reports" do
    test "each parsed sanitizer report produces a :sanitizer_report issue" do
      reports = [
        %{
          server_id: "single1",
          file: "/tmp/single1/alubsan.log.1",
          content: "ERROR: AddressSanitizer: heap-buffer-overflow",
          timestamp: nil,
          kind: "heap-buffer-overflow"
        }
      ]

      assert [issue] = Attribution.run(%Inputs{sanitizer_reports: reports}, build_windows())
      assert issue.type == :sanitizer_report
      assert issue.detail.server == "single1"
      assert issue.detail.report =~ "AddressSanitizer"
      assert issue.detail.kind == "heap-buffer-overflow"
    end

    test "a report without a timestamp is scoped to :suite" do
      reports = [
        %{
          server_id: "single1",
          file: "/tmp/single1/tsan.log.1",
          content: "WARNING: ThreadSanitizer: data race",
          timestamp: nil,
          kind: "data race"
        }
      ]

      assert [issue] = Attribution.run(%Inputs{sanitizer_reports: reports}, build_windows())
      assert issue.type == :sanitizer_report
      assert issue.scope == :suite
      assert issue.confidence == nil
    end

    test "a timestamped report is attributed via the time windows" do
      reports = [
        %{
          server_id: "single1",
          file: "/tmp/single1/alubsan.log.1",
          content: "ERROR: AddressSanitizer: use-after-free",
          timestamp: to_us(~U[2026-03-09 10:00:30Z]),
          kind: "use-after-free"
        }
      ]

      assert [issue] = Attribution.run(%Inputs{sanitizer_reports: reports}, build_windows())

      assert issue.scope == {:test, ModA, :test_one}
    end
  end

  # --- Timeouts ---

  describe "run/2 — timeout kills" do
    test "empty timeout_kills produces no timeout issues" do
      assert [] = Attribution.run(%Inputs{timeout_kills: []}, build_windows())
    end

    test "timeout kill becomes an :infrastructure issue with suite scope" do
      kill = %{
        source: :suite,
        reason: "Suite timeout exceeded",
        servers: [%{server_id: "single1", os_pid: 1001, log_file: "/tmp/single1.log"}],
        timestamp: ~U[2026-03-09 10:05:00Z]
      }

      assert [issue] = Attribution.run(%Inputs{timeout_kills: [kill]}, build_windows())
      assert issue.type == :infrastructure
      assert issue.scope == :suite
      assert issue.confidence == :high
      assert issue.detail.subtype == :timeout
      assert issue.detail.source == :suite
      assert issue.detail.reason == "Suite timeout exceeded"
      assert issue.detail.timestamp == ~U[2026-03-09 10:05:00Z]
    end

    test "the kill's already-enriched servers pass through to the issue detail" do
      # Coredump-path resolution happens in ToastTest.PostExecution.Enrichment.enrich_timeout_kills/2
      # (see EnrichmentTest); attribution just carries the enriched servers.
      kill = %{
        source: :suite,
        reason: "Suite timeout exceeded",
        servers: [
          %{
            server_id: "single1",
            os_pid: 1001,
            log_file: "/tmp/single1.log",
            coredump: "/tmp/core.1001"
          },
          %{server_id: "single2", os_pid: 1002, log_file: "/tmp/single2.log", coredump: nil}
        ],
        timestamp: ~U[2026-03-09 10:05:00Z]
      }

      assert [issue] = Attribution.run(%Inputs{timeout_kills: [kill]}, build_windows())

      by_id = Map.new(issue.detail.servers, &{&1.server_id, &1})
      assert by_id["single1"].coredump == "/tmp/core.1001"
      assert by_id["single2"].coredump == nil
    end
  end

  # --- Infrastructure events ---

  describe "run/2 — infrastructure events" do
    test "an infrastructure event is attributed by its own timestamp" do
      event = %{
        subtype: :port_exhaustion,
        timestamp: to_us(~U[2026-03-09 10:00:30Z]),
        detail: %{ports_in_use: 30_000}
      }

      assert [issue] = Attribution.run(%Inputs{infrastructure: [event]}, build_windows())
      assert issue.type == :infrastructure
      assert issue.scope == {:test, ModA, :test_one}
      assert issue.detail.subtype == :port_exhaustion
      assert issue.detail.ports_in_use == 30_000
    end
  end

  # --- Mixed ---

  describe "run/2 — mixed issues" do
    test "test failures + crash + sanitizer all combined" do
      failure = make_exunit_test(ModA, :test_one, {:failed, [{:error, %{message: "boom"}, []}]})

      crashes = [enriched_crash("single1", to_us(~U[2026-03-09 10:00:30Z]))]

      reports = [
        %{
          server_id: "single1",
          file: "/tmp/single1/alubsan.log.1",
          content: "ERROR: sanitizer report",
          timestamp: nil,
          kind: nil
        }
      ]

      inputs = %Inputs{failures: [failure], crashes: crashes, sanitizer_reports: reports}
      issues = Attribution.run(inputs, build_windows())

      types = Enum.map(issues, & &1.type)
      assert :test_failure in types
      assert :crash in types
      assert :sanitizer_report in types
      assert length(issues) == 3
    end
  end
end
