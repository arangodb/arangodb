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

defmodule ToastTest.PostExecution.EnrichmentTest do
  @moduledoc """
  Tests for the post-execution enrichment phase: turning discovered artifact
  paths into parsed data (crash log resolution + coredump analysis + sanitizer
  parsing), once, before the pure attribution step.
  """
  use ExUnit.Case, async: true

  import ToastTest.TimeTestHelpers, only: [to_us: 1]

  alias Toast.Diagnostics.Coredump.Report
  alias Toast.Process.CrashInfo
  alias ToastTest.CrashEvent
  alias ToastTest.PostExecution.Enrichment

  @tmp_dir Path.join(
             System.tmp_dir!(),
             "toast_enrichment_test_#{System.unique_integer([:positive])}"
           )
  @default_executable "/usr/bin/arangod"
  @detected_at to_us(~U[2026-03-09 10:00:30Z])

  setup do
    File.mkdir_p!(@tmp_dir)
    on_exit(fn -> File.rm_rf!(@tmp_dir) end)
    :ok
  end

  defp crash_info(overrides) do
    struct(
      %CrashInfo{
        exit_status: 139,
        signal: 11,
        executable: @default_executable,
        timestamp: @detected_at
      },
      overrides
    )
  end

  defp crash_event(server_id, info_overrides \\ []) do
    %CrashEvent{server_id: server_id, crash_info: crash_info(info_overrides)}
  end

  defp artifacts(overrides) do
    %{
      log_file: Keyword.get(overrides, :log_file),
      coredump_paths: Keyword.get(overrides, :coredump_paths, []),
      sanitizer_files: Keyword.get(overrides, :sanitizer_files, [])
    }
  end

  defp write_log(name, lines) do
    path = Path.join(@tmp_dir, name)
    File.write!(path, Enum.join(lines, "\n") <> "\n")
    path
  end

  defp log_line(time, level, topic, message) do
    %{"time" => time, "level" => level, "topic" => topic, "message" => message}
    |> :json.encode()
    |> IO.iodata_to_binary()
  end

  defp ok_report(core_path) do
    {:ok,
     %Report{
       core_path: core_path,
       binary_path: @default_executable,
       debugger: :gdb,
       signal: "SIGSEGV",
       faulting_address: nil,
       crash_thread: 1,
       threads: [%{id: 1, frames: [%{function: "boom", file: "x.cpp", line: 1}]}]
     }}
  end

  describe "enrich_crashes/3 — crash timestamp resolution" do
    test "falls back to crash_info.timestamp when no log is available" do
      events = [crash_event("single1")]

      {[enriched], _warnings} = Enrichment.enrich_crashes(events, %{}, [])

      assert enriched.effective_at == @detected_at
      assert enriched.crash_lines == nil
      assert enriched.log_file == nil
      assert enriched.coredump_reports == []
      # provenance preserved
      assert enriched.crash_info.timestamp == @detected_at
    end

    test "resolves effective_at from the crash log entry and extracts crash lines" do
      crash_us = to_us(~U[2026-03-09 10:00:28Z])

      log =
        write_log("arangod.log", [
          log_line("2026-03-09T10:00:28.000Z", "FATAL", "crash", "signal 11 received")
        ])

      events = [crash_event("single1")]
      art = %{"single1" => artifacts(log_file: log)}

      {[enriched], _warnings} = Enrichment.enrich_crashes(events, art, [])

      assert enriched.effective_at == crash_us
      assert enriched.log_file == log
      assert enriched.crash_lines =~ "signal 11 received"
      # raw detection time untouched
      assert enriched.crash_info.timestamp == @detected_at
    end
  end

  describe "enrich_crashes/3 — coredump analysis" do
    test "attaches analyzed coredump reports" do
      art = %{"single1" => artifacts(coredump_paths: ["/tmp/core.1234"])}
      events = [crash_event("single1")]

      {[enriched], _warnings} =
        Enrichment.enrich_crashes(events, art,
          analyzer: fn core, _bin, _opts -> ok_report(core) end
        )

      assert [%{core_path: "/tmp/core.1234", server_id: "single1", signal: "SIGSEGV"}] =
               enriched.coredump_reports
    end

    test "only coredumps matching the crash pid are analyzed" do
      art = %{"single1" => artifacts(coredump_paths: ["/tmp/core.1000", "/tmp/core.2000"])}
      events = [crash_event("single1", os_pid: 2000)]

      {[enriched], _warnings} =
        Enrichment.enrich_crashes(events, art,
          analyzer: fn core, _bin, _opts -> ok_report(core) end
        )

      assert [%{core_path: "/tmp/core.2000"}] = enriched.coredump_reports
    end

    test "analyzer failures are dropped, leaving the event enriched" do
      art = %{"single1" => artifacts(coredump_paths: ["/tmp/core.bad"])}
      events = [crash_event("single1")]

      {[enriched], _warnings} =
        Enrichment.enrich_crashes(events, art, analyzer: fn _, _, _ -> {:error, :no_debugger} end)

      assert enriched.coredump_reports == []
      assert enriched.effective_at == @detected_at
    end

    test "a raising analyzer degrades only the affected server, with a warning" do
      art = %{
        "a" => artifacts(coredump_paths: ["/tmp/core.a"]),
        "b" => artifacts(coredump_paths: ["/tmp/core.b"])
      }

      events = [crash_event("a"), crash_event("b")]

      analyzer = fn
        "/tmp/core.a", _bin, _opts -> raise "debugger blew up"
        core, _bin, _opts -> ok_report(core)
      end

      {enriched, warnings} = Enrichment.enrich_crashes(events, art, analyzer: analyzer)

      by_id = Map.new(enriched, &{&1.server_id, &1})
      # server a degraded but still enriched with the fallback timestamp
      assert by_id["a"].coredump_reports == []
      assert by_id["a"].effective_at == @detected_at
      # server b is unaffected
      assert [%{core_path: "/tmp/core.b"}] = by_id["b"].coredump_reports

      assert [warning] = warnings
      assert warning =~ "a"
    end
  end

  describe "sanitizer_reports/1" do
    test "reads and parses sanitizer files into plain data" do
      san = Path.join(@tmp_dir, "alubsan.log.4242")
      File.write!(san, "ERROR: AddressSanitizer: heap-buffer-overflow")

      art = %{"single1" => artifacts(sanitizer_files: [san])}

      {[report], warnings} = Enrichment.sanitizer_reports(art)

      assert warnings == []
      assert report.server_id == "single1"
      assert report.file == san
      assert report.content =~ "AddressSanitizer"
      assert report.kind == "heap-buffer-overflow"
      # no sidecar -> not attributable
      assert report.timestamp == nil
    end

    test "a single file may contain multiple reports" do
      san = Path.join(@tmp_dir, "tsan.log.7777")

      File.write!(san, """
      WARNING: ThreadSanitizer: data race one
      ==================
      WARNING: ThreadSanitizer: data race two
      """)

      art = %{"single1" => artifacts(sanitizer_files: [san])}

      {reports, _warnings} = Enrichment.sanitizer_reports(art)

      assert length(reports) == 2
      assert Enum.all?(reports, &(&1.server_id == "single1"))
    end

    test "servers without sanitizer files produce nothing" do
      {reports, warnings} = Enrichment.sanitizer_reports(%{"single1" => artifacts([])})

      assert reports == []
      assert warnings == []
    end

    test "an unreadable sanitizer file is skipped while the others still parse" do
      good = Path.join(@tmp_dir, "alubsan.log.1")
      File.write!(good, "ERROR: AddressSanitizer: heap-buffer-overflow")
      missing = Path.join(@tmp_dir, "tsan.log.2")

      art = %{"single1" => artifacts(sanitizer_files: [missing, good])}

      {reports, _warnings} = Enrichment.sanitizer_reports(art)

      assert [%{file: ^good}] = reports
    end
  end

  describe "enrich_timeout_kills/2" do
    test "attaches the first discovered coredump path to each named server" do
      art = %{
        "a" => artifacts(coredump_paths: ["/tmp/core.a1", "/tmp/core.a2"]),
        "b" => artifacts(coredump_paths: [])
      }

      kill = %{
        source: :suite,
        reason: "timeout",
        timestamp: @detected_at,
        servers: [%{server_id: "a", os_pid: 1}, %{server_id: "b", os_pid: 2}]
      }

      [enriched] = Enrichment.enrich_timeout_kills([kill], art)

      by_id = Map.new(enriched.servers, &{&1.server_id, &1})
      assert by_id["a"].coredump == "/tmp/core.a1"
      assert by_id["b"].coredump == nil
    end

    test "a server with no artifacts entry gets a nil coredump" do
      kill = %{
        source: :suite,
        reason: "timeout",
        timestamp: @detected_at,
        servers: [%{server_id: "unknown", os_pid: 9}]
      }

      [enriched] = Enrichment.enrich_timeout_kills([kill], %{})

      assert [%{server_id: "unknown", coredump: nil}] = enriched.servers
    end
  end
end
