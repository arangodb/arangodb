# Test Execution and Diagnostics

Covers test execution flow, the diagnostics pipeline, and result export.
See [architecture.md](./architecture.md) for the system overview and deployment
subsystem.


## Test Execution

### Two Execution Models

Toast supports two ways to run tests:

**Suite-based** (`mix toast`): the primary model. Multiple suites, each with
its own deployment, run sequentially via `ToastTest.Runner`.

```
mix toast suite_a suite_b
  --> Runner.run_suites([
        {SuiteA.Suite, [SuiteA.TestFoo, SuiteA.TestBar], []},
        {SuiteB.Suite, [SuiteB.TestBaz], []}
      ])
```

**Interactive** (`ToastTest.Interactive`): run individual tests against a
manually-started deployment. Useful for debugging and development.

The suite-based model exists because different test suites need different
deployment configurations (cluster topology, server args, etc.).


### Suite Structure

A suite is defined by a module using `ToastTest.Suite`:

```elixir
defmodule MySuite.Suite do
  use ToastTest.Suite, mode: :cluster, server_args: ["--query.memory-limit=1G"]

  @impl true
  def deployment_config do
    [mode: :cluster, timeout: 1_800_000, server_args: [...]]
  end
end
```

`use ToastTest.Suite` does three things:
1. Declares `@behaviour ToastTest.Suite` (requires `deployment_config/0`)
2. Generates a default `deployment_config/0` from the opts (overridable)
3. Generates a `__using__/1` macro so test modules can `use MySuite.Suite`

When a test module does `use MySuite.Suite`:
1. It expands to `use ToastTest.Case` (ExUnit.CaseTemplate)
2. Sets `@toast_suite MySuite.Suite` and defines `__toast_suite__/0`
3. This lets `ToastTest.Case.setup/1` look up the deployment from
   `DeploymentRegistry` by suite module

Optional suite callbacks:
- `setup_deployment/1` -- run after deploy, before any tests
- `teardown_deployment/1` -- run after all tests, before shutdown
- `between_tests/2` -- custom health check between tests


### Runner Lifecycle (Suite-Based)

`ToastTest.Runner.run_suites/2` processes suites sequentially:

```
For each suite:
  1. suite_module.deployment_config()
     --> extract mode, timeouts, server_args

  2. Toast.Deployment.start(mode, opts)
     --> start Controller, deploy servers, wait for health
     --> register in DeploymentRegistry

  3. suite_module.setup_deployment(deployment)  [optional]
     --> e.g., create test databases, insert seed data
     --> returns extra_context merged into test context

  4. Runner.TestExecution runs tests sequentially:
     --> start EventManager, RunnerStats, formatters
     --> for each test_module:
         a. module_started event
         b. run_setup_all (if present)
         c. for each test:
            - Runner.Timeout checks suite deadline
            - Runner.BetweenTests runs health check (or custom callback)
            - if Abort.aborted? -> skip remaining
            - Runner.TestProcess spawns test with timeout handling
            - test_finished event
         d. module_finished event
     --> suite_finished event

  5. suite_module.teardown_deployment(deployment)  [optional]

  6. Runner.PostExecution collects diagnostics:
     --> agency dump (cluster, pre-shutdown)
     --> Toast.Deployment.stop(deployment)
     --> ArtifactCollector discovers coredumps, sanitizer logs
     --> Attribution produces typed issues using TimeWindows
     --> Enrichment modules add content (logs, sanitizer reports, coredumps)
     --> Runner.ResultBuilder assembles SuiteResult

  7. StateCleanup.reset()
     --> clear ETS tables, reset formatters for next suite
```

The Runner is derived from `ExUnit.Runner` (Apache-2.0 licensed code from the
Elixir team) and reproduces its test execution mechanics. This was necessary
because ExUnit.Runner cannot be configured to:
- Deploy before running tests and collect after
- Run tests sequentially within a suite
- Perform health checks between tests
- Support the abort protocol


### Between-Test Health Checks

Between each test, the Runner calls `check_between_tests/2`:

```
check_between_tests(config, prev_test)
  |
  +-- suite_config[:between_tests] == false?
  |     --> skip (opt-out)
  |
  +-- suite_module has between_tests/2?
  |     --> call suite_module.between_tests(deployment, prev_test)
  |         return :ok or {:error, reason}
  |
  +-- default: ToastTest.Runner.BetweenTests.check(deployment, prev_test)
        --> checks controller status via Deployment.status()
        --> :ready => :ok
        --> :degraded => {:error, "servers still down..."}
        --> :failed => {:error, "Server crashed..."}
```

If the health check returns `{:error, reason}`, the Runner calls `abort!` and
skips all remaining tests. This prevents cascading failures from a crashed or
degraded deployment.

The `:degraded` check is important: if a test stops a server for chaos testing
but doesn't restart it, the deployment stays degraded. The Runner catches this
and aborts with a clear message identifying which servers are still down and
which test left them that way.


### ExUnit Compatibility Layer

`ToastTest.ExUnitCompat` wraps ExUnit internal APIs:

```
ExUnit.EventManager   -- suite/module/test lifecycle events
ExUnit.RunnerStats    -- failure counting, max_failures support
module.__ex_unit__()  -- test metadata access
```

These are private ExUnit APIs that may change between Elixir versions. The
compat layer is version-checked and warns on untested versions. Centralizing
these calls makes version upgrades a single-file change.


### Abort Protocol

The abort protocol uses an ETS table (`:toast_suite_abort`) for cross-process
signaling:

```
Runner process               Test process / CrashMonitor
     |                              |
     |  abort!(reason)              |
     |  <-------- or --------->     |
     |                              |
     |  :ets.insert_new(            |
     |    :toast_suite_abort,       |
     |    {:aborted, reason})       |
     |                              |
     |  Before each test:           |
     |  aborted?() checks ETS      |
     |  --> skip if set             |
```

`insert_new` ensures the abort message is printed exactly once even if
multiple crash callbacks fire concurrently. The ETS table is `:public` so
any process can write to it.

The abort table is cleared between suites via `clear_abort!/0`, so a crash
in suite A doesn't prevent suite B from running.


### Test Context

When a test runs, `ToastTest.Case.setup/1` provides:

```elixir
%{
  deployment: %Toast.Deployment{...},  # full deployment handle
  endpoint: "http://127.0.0.1:8529",  # primary endpoint URL
  client: %Toast.Client{...}          # pre-configured REST client
}
```

For suite-based runs, the deployment is looked up from `DeploymentRegistry`
using the test module's `__toast_suite__/0` return value. For standalone runs,
it's stored in Application env via `register_deployment/1`.


## Diagnostics Pipeline

### Overview

Diagnostics are collected post-shutdown in a multi-step protocol. The
ordering matters because some steps need live servers (agency dump) while
others need the servers stopped (coredump analysis).

```
stop_and_collect(deployment)
  |
  1. Agency dump (cluster only, pre-shutdown)
  |   GET /_api/agency/{config,state,plan} from each agent
  |   Requires agents to be alive and responsive
  |
  2. Controller.shutdown(timeout)
  |   Stop health monitors, stop server processes
  |   Collect per-server state (error, ServerInstance)
  |
  3. ArtifactCollector -- discover coredumps and sanitizer logs per server
  |   For each server with a recorded os_pid:
  |     Coredump.discover(server_dir, os_pid)
  |   Glob sanitizer log files from server directories
  |
  4. Attribution -- produce typed issues from test data + artifacts + crash events
  |   Uses TimeWindows to attribute diagnostics to tests
  |
  5. Enrichment -- enrich issues with content
  |   Enrichment.Logs: extract crash lines and time-windowed excerpts from logs
  |   Enrichment.Sanitizer: read and classify sanitizer report files
  |   Enrichment.Coredump: run debugger analysis on discovered core files
  |   Runs within coredump_timeout budget
  |
  6. Build SuiteResult with typed issues for export
```


### Crash Detection Flow

Server crashes are detected through two independent channels:

```
Channel 1: erlexec process monitoring
  erlexec monitors the OS process
  --> {:DOWN, os_pid, :process, _pid, status}
  --> ServerProcess translates to {:server_crashed, server_id, crash_info}
  --> sent to Controller (listener)
  --> Controller classifies via ServerLifecycle.handle_crash/5
  --> on_crash callback invoked for unexpected crashes

Channel 2: HTTP health monitoring
  HealthMonitor polls /_api/version every 1s
  --> 3 consecutive failures
  --> {:server_unhealthy, server_id} sent to Controller
  --> Controller kills the unresponsive server via stop_server_process
  --> on_crash callback invoked
```

Channel 1 catches process death (segfault, SIGKILL, assertion failure).
Channel 2 catches liveness failures (deadlock, resource exhaustion) where the
process is still alive but not serving requests.

After shutdown, crash details are extracted from the server log by
`Enrichment.Logs`:

```
Log line scanning:
  "{crash}" topic  --> crash_header, signal, backtrace frames
  "FATAL" level    --> fatal_lines (non-crash fatals: assertion failures, etc.)
  Time-windowed excerpts for test attribution

Output:
  %{
    signal_number: 11,
    signal_name: "SIGSEGV",
    crash_header: "caught unexpected signal 11 (SIGSEGV)",
    backtrace: ["frame 0: ...", "frame 1: ...", ...],
    fatal_lines: ["...assertion failed..."],
    crash_output: [...all {crash} lines...],
    timestamp: ~U[2026-02-28 10:30:45Z]
  }
```


### Sanitizer Error Collection

Sanitizer integration has three phases:

```
Phase 1: Detection (Config.load)
  detect_from_build_dir(build_dir)  -- infer "alubsan"/"tsan" from path
  Sanitizer.detect(explicit)        -- check env vars or force type
  --> MapSet of active env var names (e.g., #MapSet<["ASAN_OPTIONS", ...]>)

Phase 2: Environment setup (Factory.build_*)
  Sanitizer.build_env(active, log_dir, repo_root, explicit)
  --> for each active var:
      build base options (halt_on_error=0, etc.)
      merge user env var values (user overrides base)
      add log_path=<server_dir>/alubsan.log (or tsan.log)
      add suppressions file if exists
  --> returns [{var_name, "key=val:key=val:..."}]
  --> passed to erlexec as process environment

Phase 3: Post-shutdown collection
  Sanitizer.collect_errors(server_dir, server_id)
  --> glob server_dir/alubsan.log.* and tsan.log.*
      (sanitizers create one file per report with PID suffix)
  --> filter out empty files (< 10 bytes)
  --> read content, get mtime as timestamp
  --> returns [%{content, file_path, timestamp, sanitizer_type, server_id}]
```

The `halt_on_error=0` default is important: it tells sanitizers to log errors
and continue rather than aborting the process. This way all sanitizer errors
during a test run are collected, not just the first one.


### Coredump Analysis

Core dump discovery searches multiple locations because the core file location
depends on OS and kernel configuration:

```
discover(server_dir: dir, os_pid: pid)
  |
  +-- TOAST_COREDUMP_DIR set?
  |     --> search only that directory
  |
  +-- otherwise, search all:
      1. cores_in_dir(server_dir)  -- core* in the server's work dir
      2. cores_in_tmp(os_pid)      -- /tmp/core* matching the PID
      3. cores_from_pattern(os_pid)
           read /proc/sys/kernel/core_pattern
           +-- starts with "|" --> coredumpctl (systemd)
           +-- otherwise --> expand %p/%e/etc. and glob
```

Analysis runs a debugger (LLDB preferred, GDB fallback) with a timeout:

```
analyze(core_path, binary_path, opts)
  --> detect_debugger() or use configured one
  --> debugger.command(binary_path, core_path) -- build CLI args
  --> Port.open + collect output with deadline
  --> debugger.parse_output(output)
  --> build Report struct

Debugger behaviour callbacks:
  executable/0  -- "lldb" or "gdb"
  command/2     -- build debugger-specific CLI args
  parse_output/1 -- parse signal, threads, backtrace from output
```

The `Coredump.collect/1` function distributes a total timeout budget across
all servers and core files. If analysis of one core takes too long, it's
killed and the remaining budget is used for the next.


### Test Attribution

After artifacts are collected, `ToastTest.Attribution` orchestrates issue
production from test data, artifacts, and crash events. It uses
`ToastTest.Attribution.TimeWindows` to attribute diagnostics to specific tests
using timestamps:

```
Attribution produces SuiteResult issues from:
  - test results (from ResultCollector)
  - artifacts (from ArtifactCollector: coredumps, sanitizer logs)
  - crash events (from Controller)

TimeWindows.attribute(items, test_results)
  |
  For each item (crash or sanitizer error) with a timestamp:
    For each test with started_at / finished_at:
      calculate_confidence(timestamp, started_at, finished_at)
        --> :high if timestamp in [started_at, finished_at]
        --> :low  if timestamp in (finished_at, finished_at + 5s]
        --> :none otherwise
    Take best match (:high stops search, :low continues looking)
  |
  Returns attributed and unattributed items
```

The 5-second tolerance for `:low` confidence accounts for:
- Sanitizer log file mtimes being slightly after the write
- Crashes that occur during test teardown / on_exit handlers
- Clock granularity between Elixir monotonic time and OS timestamps

`Attribution` handles both crash and sanitizer attribution internally,
producing typed issues for the `SuiteResult`.


### SuiteResult Data Shape

The diagnostics pipeline produces a `ToastTest.SuiteResult` struct with typed
issues rather than a raw diagnostics map. Issues are produced by `Attribution`
and enriched by the `Enrichment.*` modules. The `SuiteResult` is the central
data structure consumed by both CLI output (`PostExecSummary`) and export
(`SuiteResult.JSON`, `SuiteResult.JUnitXML`).


## Results and Export

### Result Collection Pipeline

```
Test execution
  |
  +--> Formatting.CLI (GenServer)
  |      Receives ExUnit events
  |      Prints Google Test-style output with timestamps
  |
  +--> ResultCollector (GenServer)
  |      Receives ExUnit events
  |      Collects structured test results
  |
  +--> EventStore
  |      Records deployment lifecycle events (server start/stop/crash)
  |
  +--> After suite completes (Runner.PostExecution):
         agency dump (cluster, pre-shutdown)
         Toast.Deployment.stop(deployment)
         ArtifactCollector -- discover coredumps, sanitizer logs per server
         Attribution -- produce typed issues using TimeWindows
         Enrichment.Logs -- extract crash lines, log excerpts
         Enrichment.Sanitizer -- read and classify sanitizer reports
         Enrichment.Coredump -- run debugger analysis on core files
         Runner.ResultBuilder -- assemble SuiteResult
         Formatting.PostExecSummary -- CLI output
         SuiteResult.JSON.render(suite_result) --> results.json
         SuiteResult.JUnitXML.render(suite_result) --> results.xml
         .diagnostics.etf --> serialized diagnostics for offline analysis
         --> writes to result_dir
```

### CI Result Tiers

`ToastTest.ResultPackaging` organizes artifacts by importance:

| Tier | Contents | Behavior |
|------|----------|----------|
| 1 | results.json, results.xml, toast.log | Always published |
| 2 | Server logs, sanitizer reports | Compressed into toast-logs.tar.gz |
| 3 | Core dump files, work directory archive | Individually compressed (zstd preferred, gzip fallback) |

### Exit Codes

`ResultPackaging.exit_code/1` returns a monotonic severity-ordered exit code:

| Code | Meaning | Severity |
|------|---------|----------|
| 0 | All tests passed | -- |
| 1 | Test failures | Lowest |
| 2 | Sanitizer errors detected | Medium |
| 3 | Infrastructure failure (deploy failed, etc.) | High |
| 4 | Server crash | Highest |

The ordering prioritizes crashes (4) over infrastructure (3) over sanitizer (2)
over test failures (1). A run with both a crash and sanitizer errors returns 4.
