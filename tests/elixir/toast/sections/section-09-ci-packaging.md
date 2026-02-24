Now I have a thorough understanding of the existing codebase and plan. Let me compose the section.

# Section 9: CI Packaging and Analysis

## Overview

This section implements tiered result packaging for CI environments, a post-run analysis CLI tool, and exit code management. It builds on the runner (section 05) for cross-suite result aggregation and diagnostics collection (section 08) for agency dumps and coredump reports.

**Dependencies**: section-05-runner (cross-suite result aggregation, runner exit flow), section-08-diagnostics (coredump reports, agency dumps integrated into `stop_and_collect/1`)

**Files to create**:
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/result_packaging.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/analysis/summary.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/analysis/failures.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/analysis/crashes.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/analysis/performance.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/mix/tasks/toast.analyze.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/result_packaging_test.exs`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/analysis/summary_test.exs`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/analysis/failures_test.exs`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/analysis/crashes_test.exs`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/analysis/performance_test.exs`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/mix/tasks/toast_analyze_test.exs`

**Files to modify**:
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/mix/tasks/toast.ex` (add `--ci` flag, exit code strategy)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter.ex` (renamed from `Toast.ResultExporter`, extended with suite-level grouping)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/config.ex` (add CI-related config keys)

---

## Tests

Tests are written first and drive the implementation. All tests run with `mix test` -- no running ArangoDB required.

### Result Packaging Tests

```elixir
# test/toast/result_packaging_test.exs
defmodule Toast.ResultPackagingTest do
  use ExUnit.Case, async: true

  alias Toast.ResultPackaging

  # --- Local vs CI behavior ---

  # Test: local run (no --ci) produces no packaging, prints summary and work dir path
  # Test: CI run produces tier 1 files (results.json, results.xml, toast.log)
  # Test: CI run produces tier 2 archive (toast-logs.tar.gz with server logs,
  #       sanitizer reports, crash reports, agency dumps)
  # Test: CI run produces tier 3 files (individually compressed core dumps)
  # Test: tier 3 only created when crashes exist
  # Test: zstd compression used; gzip fallback if zstd unavailable

  # --- Exit codes ---

  # Test: exit code 0 for all passed
  # Test: exit code 1 for test failures
  # Test: exit code 2 for infrastructure failure
  # Test: exit code 3 for server crash
  # Test: exit code 4 for sanitizer-only errors
  # Test: mixed results -> highest severity exit code wins (3 > 2 > 4 > 1 > 0)
end
```

### Analysis Tool Tests

```elixir
# test/mix/tasks/toast_analyze_test.exs
defmodule Mix.Tasks.Toast.AnalyzeTest do
  use ExUnit.Case, async: true

  # Test: mix toast.analyze reads results.json and prints summary
  # Test: --failures shows detailed failure info with stack traces
  # Test: --crashes shows crash diagnostics, sanitizer errors, coredump traces
  # Test: --slow N shows N slowest tests with durations
  # Test: invalid file path produces clear error
  # Test: malformed JSON produces clear error
end
```

### Result Export Tests (Suite-Level Grouping)

```elixir
# test/toast_test/result_exporter_test.exs (extended)

  # Test: results include suite-level grouping
  # Test: global summary aggregates across suites
  # Test: per-suite and per-test timing included
  # Test: JUnit XML includes suite-level <testsuite> elements
```

---

## Implementation Details

### Result Packaging (`Toast.ResultPackaging`)

The `Toast.ResultPackaging` module handles the CI-specific artifact creation. It is invoked by the `mix toast` task after all suites complete.

#### CI Mode Detection

CI mode is activated by either:
- The `--ci` CLI flag on `mix toast`
- The `TOAST_CI=true` environment variable

When CI mode is not active (local development), result packaging is skipped entirely. The framework prints a test summary to stdout and the path to the work directory for manual inspection. The analysis tool (`mix toast.analyze`) can read `results.json` directly from the work directory.

Add these to the existing `@switches` list in `Mix.Tasks.Toast` and to the `@toast_env_map`:
- `ci: :boolean` switch
- `"TOAST_CI"` env var mapping

#### Tiered Packaging

Packaging is organized into three tiers by size and frequency of need.

**Tier 1 -- Always Published (small, always needed)**

Published directly as CI artifacts without archiving. These are the files CI systems consume:
- `results.json` -- Full structured results with suite-level grouping (produced by `ToastTest.ResultExporter`)
- `results.xml` -- JUnit XML for CI test reporting (produced by `ToastTest.ResultExporter`)
- `toast.log` -- Framework debug log (the Logger output file)

Tier 1 files are copied from the work directory to the result directory. They already exist from the result export step; packaging just ensures they are in the expected location.

**Tier 2 -- Compressed Archive (medium, usually needed)**

Packed into a single compressed archive `toast-logs.tar.gz` in the result directory:
- Server log files (per-server, from each suite's work directory)
- Sanitizer reports (ASAN/LSAN/UBSAN/TSAN log files)
- Crash reports (extracted stack traces from debugger, if section 08 is implemented)
- Agency dumps (JSON files from cluster diagnostics, if section 08 is implemented)

Implementation: Use `System.cmd("tar", ["czf", archive_path | file_list])` or fall back to Erlang's `:erl_tar` module. The file list is gathered by walking each suite's diagnostics data, which includes paths to log files, sanitizer reports, and crash reports.

**Tier 3 -- Individually Compressed (large, rarely needed)**

Each file compressed individually, published as separate artifacts:
- Core dumps (e.g., `core.12345.zst`)
- Full database directories (if `--keep-work-dir` is set)

Tier 3 artifacts are only created when they exist (crashes are not the common case).

Compression preference: zstd for speed and compression ratio. The module checks for zstd availability via `System.find_executable("zstd")`. If unavailable, falls back to gzip. Core dumps can be large (gigabytes), so zstd's speed advantage matters.

```elixir
defmodule Toast.ResultPackaging do
  @moduledoc """
  Tiered result packaging for CI environments.

  Tier 1: Always published (results.json, results.xml, toast.log)
  Tier 2: Compressed archive (server logs, sanitizer reports, crash reports, agency dumps)
  Tier 3: Individually compressed (core dumps, database dirs)
  """

  @doc "Package results for CI upload. No-op when ci_mode is false."
  @spec package(keyword()) :: :ok
  def package(opts)
  # opts includes :ci (boolean), :result_dir, :work_dir, :suite_diagnostics (list of per-suite diagnostics)

  @doc "Determine if zstd is available for compression."
  @spec zstd_available?() :: boolean()
  def zstd_available?()

  @doc "Compress a file with zstd, falling back to gzip."
  @spec compress_file(Path.t(), Path.t()) :: {:ok, Path.t()} | {:error, term()}
  def compress_file(source, dest)
end
```

#### Exit Code Strategy

The `mix toast` task exits with a status code that communicates the nature of the result. Exit codes are ordered by severity so that mixed results across suites produce the most severe code.

| Code | Meaning | Severity |
|------|---------|----------|
| 0 | All tests passed, no sanitizer errors | lowest |
| 1 | Test failures (some tests failed, but no infrastructure issues) | |
| 4 | Sanitizer errors (all tests passed, but ASAN/TSAN/UBSAN reported issues) | |
| 2 | Infrastructure failure (deployment could not start, global timeout exceeded) | |
| 3 | Server crash (server crashed unexpectedly during testing) | highest |

The severity ordering for mixed results is: **3 > 2 > 4 > 1 > 0**. When multiple suites produce different outcomes, the highest-severity exit code wins.

Implementation: After all suites complete, the runner returns aggregated stats that include:
- `failures` count (test failures)
- `aborted?` flag and abort reason (crash or infrastructure)
- sanitizer error presence (from diagnostics)
- deployment failure flag

A function `Toast.ResultPackaging.exit_code/1` computes the appropriate code:

```elixir
@doc "Compute exit code from aggregated run results."
@spec exit_code(map()) :: 0 | 1 | 2 | 3 | 4
def exit_code(results)
# results is a map with keys:
#   :test_failures (integer)
#   :server_crashed (boolean)
#   :infrastructure_failure (boolean)
#   :sanitizer_errors (boolean)
```

The `mix toast` task currently uses `exit({:shutdown, exit_status})` via `System.at_exit`. The refactored version replaces the hardcoded exit status `2` with the computed exit code. The `--ci` flag does not affect exit codes -- exit codes are always set appropriately regardless of CI mode. CI mode only controls whether packaging is performed.

### Analysis Tool (`mix toast.analyze`)

A new mix task that reads `results.json` and provides formatted post-run analysis. This is decoupled from test execution -- it operates on result files only.

```
mix toast.analyze results.json              # Summary view (default)
mix toast.analyze results.json --failures   # Detailed failure info
mix toast.analyze results.json --crashes    # Crash diagnostics
mix toast.analyze results.json --slow 10    # Slowest N tests
```

#### Mix Task Implementation

```elixir
defmodule Mix.Tasks.Toast.Analyze do
  @shortdoc "Analyze Toast test results"
  @moduledoc """
  Post-run analysis of Toast test results.

  ## Usage

      mix toast.analyze <results.json> [options]

  ## Options

      --failures    Show detailed failure info with stack traces
      --crashes     Show crash diagnostics, sanitizer errors, coredump traces
      --slow N      Show N slowest tests with durations (default: 10)
  """

  use Mix.Task

  @switches [
    failures: :boolean,
    crashes: :boolean,
    slow: :integer
  ]

  @impl Mix.Task
  def run(args)
end
```

The task reads the JSON file, parses it, and delegates to analysis modules based on flags. If no flags are provided, the summary view is shown by default.

Error handling:
- Invalid file path: print "Error: file not found: <path>" and exit with status 1
- Malformed JSON: print "Error: invalid JSON in <path>: <reason>" and exit with status 1

#### Analysis Modules

Each module reads the parsed `results.json` map and produces formatted terminal output.

**`Toast.Analysis.Summary`** -- Pass/fail counts, durations, suite breakdown.

```elixir
defmodule Toast.Analysis.Summary do
  @moduledoc "Format test result summary for terminal output."

  @doc "Format a summary from parsed results.json data."
  @spec format(map()) :: String.t()
  def format(results)
  # Outputs:
  #   Total: N tests, N passed, N failed, N skipped
  #   Duration: Xm Ys
  #   Suite breakdown:
  #     smoke: 10 passed, 0 failed (12.3s)
  #     shell_server: 45 passed, 2 failed (1m 23.4s)
end
```

**`Toast.Analysis.Failures`** -- Failure messages, stack traces, related diagnostics.

```elixir
defmodule Toast.Analysis.Failures do
  @moduledoc "Format detailed failure information for terminal output."

  @doc "Format failure details from parsed results.json data."
  @spec format(map()) :: String.t()
  def format(results)
  # For each failed test, outputs:
  #   Module - test name
  #     file:line
  #     Failure message
  #     Stack trace (truncated)
  #     Related diagnostics (sanitizer errors, crash reports matched to this test)
end
```

**`Toast.Analysis.Crashes`** -- Crash reports, sanitizer errors, coredump traces.

```elixir
defmodule Toast.Analysis.Crashes do
  @moduledoc "Format crash diagnostics for terminal output."

  @doc "Format crash and sanitizer information from parsed results.json data."
  @spec format(map()) :: String.t()
  def format(results)
  # Outputs:
  #   Crash reports (from crash_matching and server_health)
  #   Sanitizer errors (from sanitizer_matching)
  #   Coredump traces (from coredump_reports, if present)
end
```

**`Toast.Analysis.Performance`** -- Slowest tests, duration distribution.

```elixir
defmodule Toast.Analysis.Performance do
  @moduledoc "Format performance analysis for terminal output."

  @doc "Format slowest tests. n defaults to 10."
  @spec format(map(), non_neg_integer()) :: String.t()
  def format(results, n \\ 10)
  # Outputs:
  #   Slowest N tests:
  #     1. Module - test name (12.345s)
  #     2. Module - test name (8.901s)
  #     ...
  #   Duration distribution:
  #     <1s: N tests
  #     1-5s: N tests
  #     5-30s: N tests
  #     >30s: N tests
end
```

### Result Export Updates (Suite-Level Grouping)

The existing `Toast.ResultExporter` (renamed to `ToastTest.ResultExporter` in section 02) needs extension for suite-level grouping. Currently, it groups tests by module. The new structure groups by suite first, then by module within each suite.

The `results.json` structure changes from:

```json
{
  "test_suites": {
    "Elixir.Smoke.VersionTest": { ... }
  }
}
```

to:

```json
{
  "suites": {
    "smoke": {
      "deployment_mode": "single_server",
      "duration_seconds": 12.345,
      "diagnostics": { ... },
      "summary": { "total": 10, "passed": 10, ... },
      "test_modules": {
        "Elixir.Smoke.VersionTest": {
          "tests": [ ... ]
        }
      }
    }
  },
  "summary": {
    "total": 55,
    "passed": 53,
    "failed": 2,
    ...
  }
}
```

The JUnit XML similarly gains suite-level `<testsuite>` elements, one per Toast suite (not per module). Each `<testsuite>` element contains `<testcase>` elements for all tests in that suite. The `classname` attribute uses the module name for traceability.

These changes are driven by the cross-suite stats aggregation from section 05. The runner passes per-suite results (including suite name, deployment mode, diagnostics, and test results) to the result exporter.

### Integration with `mix toast` Task

After all suites complete, the `mix toast` task:

1. Calls `ToastTest.ResultExporter.export/1` to write `results.json` and `results.xml`
2. If CI mode is active, calls `Toast.ResultPackaging.package/1` to create tiered artifacts
3. Computes exit code via `Toast.ResultPackaging.exit_code/1`
4. Prints summary to stdout (always, regardless of CI mode)
5. Exits with the computed exit code

The `--ci` flag is added to the `mix toast` task's switch list:

```elixir
@switches [
  # ... existing switches ...
  ci: :boolean
]
```

And to the env var map:

```elixir
@toast_env_map %{
  # ... existing mappings ...
  ci: "TOAST_CI"
}
```

### CircleCI Integration Configuration

The packaging structure is designed for CircleCI's artifact system. A typical `.circleci/config.yml` job would use:

```yaml
- store_test_results:
    path: toast-results/results.xml
- store_artifacts:
    path: toast-results/
    destination: toast
```

Where `toast-results/` contains:
- `results.json` (Tier 1)
- `results.xml` (Tier 1)
- `toast.log` (Tier 1)
- `toast-logs.tar.gz` (Tier 2)
- `core.*.zst` (Tier 3, if any)

The `store_test_results` step reads `results.xml` for CircleCI's built-in test reporting (showing pass/fail in the UI, tracking flaky tests). The `store_artifacts` step uploads everything for download.

No actual CircleCI configuration file is created by this section -- the CI configuration lives in the repository's `.circleci/` directory and is maintained separately. This section only ensures the output format is compatible.

**Known limitation**: Long-running suites can produce multi-GB server logs. Tier 2 compression may time out or OOM on very large log sets. This is acknowledged as a future concern — consider per-suite log rotation or log size caps if it becomes an issue in practice.

---

## Migration Verification (Phase 5)

This section corresponds to Phase 5 steps 5-8 of the migration plan:

- **Step 5**: Implement tiered result packaging (Tier 1 always published, Tier 2 compressed archive including agency dumps, Tier 3 individually compressed large files)
- **Step 6**: Implement `--ci` flag / `TOAST_CI` env var to toggle packaging behavior (local vs CI)
- **Step 7**: Define exit code strategy (0-4 with severity ordering)
- **Step 8**: Implement `mix toast.analyze` with summary, failures, crashes, and performance views

Verification criteria:
- `mix toast --ci` produces tiered packages in the result directory
- `mix toast.analyze results.json` prints a human-readable summary
- `mix toast.analyze results.json --failures` shows detailed failure info
- `mix toast.analyze results.json --crashes` shows crash diagnostics
- `mix toast.analyze results.json --slow 5` shows the 5 slowest tests
- Exit codes correctly reflect run outcomes (test with each failure type)
- Mixed results across suites produce the highest-severity exit code