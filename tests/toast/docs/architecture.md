# TOAST Architecture

Technical design documentation for developers and LLMs working on the TOAST
test framework internals.

TOAST (Toolkit for Arango System Testing) is an integration testing framework
for ArangoDB. It manages server
deployments (single server and cluster), runs tests against them, and provides
comprehensive diagnostics when things go wrong.

**See also**:
- [Test Execution and Diagnostics](./test-execution-and-diagnostics.md)
- [Data Structures and Dependencies](./data-structures-and-dependencies.md)


## System Overview

Toast has eight major subsystems:

```
+---------------------------------------------------------------------+
|                          mix toast (CLI)                             |
+---------------------------------------------------------------------+
         |                    |                       |
         v                    v                       v
  +-------------+    +------------------+    +------------------+
  | Toast.Env   |    | ToastTest.Runner |    | ResultPackaging  |
  +-------------+    +------------------+    +------------------+
                              |
              +---------------+---------------+
              |               |               |
              v               v               v
  +-----------------+  +--------------+  +------------------------+
  | Deployment      |  | Attribution  |  | Diagnostics            |
  | (Controller,    |  | & Enrichment |  | (Coredump, Sanitizer,  |
  |  DeployPipeline,|  | (Attribution,|  |  AgencyDump)           |
  |  ShutdownPipe,  |  |  Logs,       |  +------------------------+
  |  Factory,       |  |  Sanitizer,  |           ^
  |  Health)        |  |  Coredump,   |           |
  +-----------------+  |  PostExec    |     (uses low-level
              |        |  Summary)    |      analysis tools)
              |        +--------------+
              v
  +-------------------------+
  | Process                 |
  | (ServerProcess,         |
  |  HealthMonitor)         |
  +-------------------------+
              |
              v
  +-------------------------+
  | erlexec (OS processes)  |
  +-------------------------+
```

Data flows top-to-bottom during setup and test execution:

1. CLI parses args, loads config via `Toast.Env`, discovers suites
2. Runner starts a deployment per suite via `Toast.Deployment.start/3`
3. Controller (GenServer) orchestrates server launch via DeployPipeline + Factory + ServerProcess
4. Tests run against the deployment via Client REST wrappers
5. On completion, artifacts are collected and attributed to tests via Attribution + Enrichment
6. Results are structured as SuiteResult, exported to JSON/XML, and optionally packaged for CI


## OTP Supervision Tree

```
Toast.Supervisor (one_for_one)
|
+-- Toast.PortAllocator (GenServer)
|     Allocates available TCP ports via socket probing.
|     Stateful counter starting from a random base port.
|
+-- ToastTest.EventStore (GenServer)
|     ETS-backed event store for deployment lifecycle events.
|     Chronologically ordered by {timestamp, sequence}.
|
+-- Toast.Process.Supervisor (DynamicSupervisor, max_restarts: 0)
|     |
|     +-- Toast.Process.ServerProcess (GenServer)  -- one per server
|     |     Manages a single OS process via erlexec.
|     |     Reports crashes to its listener (the Controller).
|     |
|     +-- Toast.Process.HealthMonitor (GenServer)  -- one per server
|           Periodically polls /_api/version to detect
|           unresponsive servers.
|
+-- Toast.Deployment.Supervisor (DynamicSupervisor, max_restarts: 0)
      |
      +-- Toast.Deployment.Controller (GenServer)  -- one per deployment
            Orchestrates deploy/shutdown for one deployment.
            Uses DeployPipeline for startup, ShutdownPipeline for teardown.
```

Design decisions:

- **`max_restarts: 0`** on both DynamicSupervisors: deployments and server
  processes should never auto-restart. Crashes are reported to the Controller
  for deliberate handling (abort tests, collect diagnostics). Automatic restart
  would mask real failures.

- **Flat DynamicSupervisor** instead of per-deployment supervisor trees: a
  deployment Controller manages N ServerProcess children by pid reference. This
  avoids nested supervisor complexity since the Controller already tracks all
  server state. The DynamicSupervisor is just a container for clean BEAM-level
  shutdown.

- **PortAllocator as a GenServer**: port allocation must be serialized to avoid
  races between concurrent deployments. The GenServer holds a monotonic counter
  and probes each candidate via `:gen_tcp.listen/2`.


## Module Map

### Configuration

| Module | Role |
|--------|------|
| `Toast.Env` | Resolve configuration from env vars, `.toast.local.exs`, and CLI opts into Application env. Single source of truth for configuration resolution and precedence. |
| `Toast.Deployment.Config` | Deployment infrastructure config: build dir, server args, sanitizers, timeouts, cluster topology, JWT, SSL. Constructed from Application env via `new/1`. |
| `ToastTest.Config` | Test execution config: result dirs, diagnostics, CI settings, debugger. Intentionally separate from Deployment.Config. |

### Deployment (`Toast.Deployment.*`)

| Module | Role |
|--------|------|
| `Toast.Deployment` | Public API facade. Start/stop deployments, query status, server control ops, client creation. |
| `Toast.Deployment.ServerInfo` | Immutable snapshot of server info (id, role, port, endpoint, arango_id). Safe to pass around in test context. |
| `Toast.Deployment.Controller` | GenServer. Shared state machine for deployment lifecycle, server control, crash handling. |
| `Toast.Deployment.Controller.State` | Internal state struct for Controller (nested module). |
| `Toast.Deployment.DeployPipeline` | Multi-phase deploy sequence: start processes, launch by role group, health check, post-deploy. |
| `Toast.Deployment.ShutdownPipeline` | Ordered shutdown, rollback, and abort sequences. |
| `Toast.Deployment.Factory` | Builds launch specs (executable path, args, env, dirs) from Config. |
| `Toast.Deployment.CommandBuilder` | Generates arangod CLI arguments per role. |
| `Toast.Deployment.Health` | HTTP health check polling (`/_api/version`, `/_api/agency/config`). |
| `Toast.Deployment.ServerLifecycle` | Pure functions for server ops (stop, kill, pause, resume, relaunch, crash handling). Used by Controller. |
| `Toast.Deployment.FailurePoint` | Failure injection via `/_admin/debug/failat` REST endpoints. |
| `Toast.Deployment.CrashExpectation` | Data struct for tracking expected crashes (timer, crash_info, waiter). |
| `Toast.Deployment.ServerInstance` | Data struct: runtime state of one server (ports, pids, endpoints, operational state). |
| `Toast.Deployment.ClusterOpts` | Data struct: cluster topology and per-role arguments. |
| `Toast.Deployment.Events` | Event emission for deployment lifecycle. Single source of truth for event format. |
| `Toast.Deployment.EventListener` | Behaviour for receiving lifecycle events (on_event, on_crash). |
| `Toast.Deployment.DefaultEventListener` | No-op implementation of EventListener. |
| `Toast.Deployment.CrashBarrier` | Between-tests barrier that checks `/proc/<pid>/status` to detect in-flight crashes before the next test starts. |
| `Toast.Deployment.HealthBarrier` | Between-tests barrier that waits for each server's HealthMonitor to report healthy before the next test starts. Catches "alive but unresponsive" cases (deadlocks, resource exhaustion). |
| `Toast.Deployment.Netstat` | Between-tests port exhaustion check. Counts system TCP sockets via `ss`/`netstat` (~2ms fast path); gathers per-server PID-based breakdown (~40ms) only on threshold breach. Two thresholds: system-wide (15k) and per-deployment (500/server above baseline). |
| `Toast.Deployment.Supervisor` | DynamicSupervisor for Controller processes. |

### Process Management (`Toast.Process.*`)

| Module | Role |
|--------|------|
| `Toast.Process.ServerProcess` | GenServer wrapping erlexec for one OS process. Lifecycle + crash detection. |
| `Toast.Process.HealthMonitor` | GenServer polling HTTP health. Notifies listener after N consecutive failures. |
| `Toast.Process.CrashInfo` | Data struct for crash information (exit status, signal, timestamp, PID). |
| `Toast.Process.ProcStatus` | Reads `/proc/<pid>/status` to check process state (running, zombie, etc.). Used by CrashBarrier. |
| `Toast.Process.Supervisor` | DynamicSupervisor for ServerProcess and HealthMonitor children. |

### JWT (`Toast.JWT.*`)

| Module | Role |
|--------|------|
| `Toast.JWT` | JWT token generation and signing. |
| `Toast.JWT.KeyGen` | Generate JWT signing material and keyfiles (HMAC and ECDSA). |
| `Toast.JWT.Provider` | Data struct holding a JWT signer. Used by Deployment to authenticate clients. |

### Test Execution (`ToastTest.*`)

| Module | Role |
|--------|------|
| `ToastTest.Runner` | Suite-based test runner (derived from ExUnit.Runner). Orchestrates deploy -> run -> collect. |
| `ToastTest.Runner.TestExecution` | Core test execution loop (setup_all, per-test spawn, result handling). |
| `ToastTest.Runner.TestProcess` | Spawn and manage individual test processes with timeout handling. |
| `ToastTest.Runner.TestFilter` | Filter test modules by line number, test name, and ExUnit tags. |
| `ToastTest.Runner.BetweenTests` | Between-test checks with four phases: (1) `CrashBarrier.await_settled/2` checks for in-flight crashes, (2) `HealthBarrier.await_healthy/2` waits for health monitors, (3) `Netstat.check/3` checks port exhaustion, (4) suite's custom `between_tests/2` callback (or default `BetweenTests.check/2`). |
| `ToastTest.Runner.PostExecution` | Post-suite diagnostics collection (agency dump, artifact collection, attribution). |
| `ToastTest.Runner.ResultBuilder` | Build SuiteResult from test data, diagnostics, and enrichment. |
| `ToastTest.Runner.FailureFormatter` | Format test failures for display. |
| `ToastTest.Runner.Timeout` | Timeout calculation and enforcement for suite/test deadlines. |
| `ToastTest.Suite` | Behaviour + `__using__` macro for suite definition. Defines `deployment_config/0`. |
| `ToastTest.Case` | ExUnit.CaseTemplate providing `deployment`, `endpoint`, `client` context. |
| `ToastTest.Expect` | Non-fatal expectation macro (like Google Test's EXPECT_*). |
| `ToastTest.ExUnitCompat` | Shim over ExUnit internal APIs (EventManager, RunnerStats). |
| `ToastTest.SuiteRun` | Data struct: per-suite execution context (module, deployment, deadline). |
| `ToastTest.DeploymentRegistry` | ETS-based mapping of suite modules to active deployments. |
| `ToastTest.DeploymentListener` | EventListener implementation delegating to EventStore and CrashMonitor. |
| `ToastTest.CrashMonitor` | Default crash callback; calls `Runner.abort!`. |
| `ToastTest.CrashEvent` | Data struct for crash event metadata (constructed and consumed by test execution layers). |
| `ToastTest.EventStore` | ETS-backed event store for lifecycle events. Chronologically ordered. |
| `ToastTest.EventStore.Projections` | Pure-functional projections over the event stream (e.g., pids_by_server). |
| `ToastTest.Events` | Public API for emitting custom events into the test event store. |
| `ToastTest.Abort` | ETS-backed suite-level abort state with human-readable messages. |
| `ToastTest.StateCleanup` | Resets shared state between suite runs (ETS tables, formatters, callbacks). |
| `ToastTest.TestLifecycle` | Shared test lifecycle primitives (spawn_setup_all, spawn_test, etc.) used by both Interactive and Runner. |
| `ToastTest.Interactive` | Run individual tests against an existing deployment (debugging). |
| `ToastTest.DebuggerAttach` | Print debugger attach commands and wait for user input (--attach-debugger). When active, test timeouts are disabled. |
| `ToastTest.WithDeployment` | Helper for starting scoped deployments within individual test cases. Used in `mode: :manual` suites. |
| `ToastTest.DeployConfig` | Builds `Toast.Deployment.Config` from flat suite-style options. Used by both Runner and WithDeployment. |
| `ToastTest.DeploymentEventListener` | Event listener for per-test deployments (manual mode). Records events but does not abort on crashes. |
| `ToastTest.TimeoutError` | Custom exception for test/suite timeout conditions. |

### JavaScript Test Integration (`ToastTest.JS.*`)

| Module | Role |
|--------|------|
| `ToastTest.JavascriptSuite` | Suite macro for JS test suites. Extends `ToastTest.Suite` with JS-specific config (paths, extra args, weights). |
| `ToastTest.JS.ModuleBuilder` | Generates Elixir modules from JS file paths. Each JS file becomes a module with `__ex_unit__/0`, `__toast_js_file__/0`, and `__toast_weight__/0`. Derives ExUnit tags from filename suffixes (e.g., `-cluster` → `:cluster_only`). |
| `ToastTest.JS.TestRunner` | Runs a JS test module by delegating to an Executor, then emitting ExUnit-compatible events. Uses the JS file path atom as the module name in ExUnit events so results are keyed by file path. |
| `ToastTest.JS.Executor` | Behaviour for JS test execution backends. Defines `run(js_file, opts)` callback. |
| `ToastTest.JS.ArangoshExecutor` | Executor that launches arangosh with `--javascript.unit-tests`, parses `testresult.json` output. |
| `ToastTest.JS.ResultMapper` | Maps JSON results from JS execution to `ExUnit.Test`/`ExUnit.TestModule` structs for integration with the result collection pipeline. |

### Diagnostics (`Toast.Diagnostics.*`)

Lower-level infrastructure for sanitizer detection, core dump analysis, and
agency state capture. Higher-level attribution and enrichment logic lives in
`ToastTest.Attribution` and `ToastTest.Enrichment.*`.

| Module | Role |
|--------|------|
| `Toast.Diagnostics.Sanitizer` | Detect sanitizers, build env vars, collect log files post-shutdown. |
| `Toast.Diagnostics.Coredump` | Discover core dumps, run GDB/LLDB analysis. |
| `Toast.Diagnostics.Coredump.Debugger` | Behaviour for debugger backends + shared frame filtering. |
| `Toast.Diagnostics.Coredump.GDB` | GDB-specific command building and output parsing. |
| `Toast.Diagnostics.Coredump.LLDB` | LLDB-specific command building and output parsing. |
| `Toast.Diagnostics.Coredump.Report` | Data struct: structured coredump analysis result. |
| `Toast.Diagnostics.AgencyDump` | Capture agency config/state/plan from live agents (cluster only). |

### Attribution & Enrichment (`ToastTest.Attribution.*`, `ToastTest.Enrichment.*`)

Orchestrates issue production from test data, artifacts, and deployment errors.
`ArtifactCollector` discovers filesystem artifacts, `Attribution` combines them
with test results into `SuiteResult` issues, and `Enrichment.*` modules handle
reading/parsing specific artifact types.

| Module | Role |
|--------|------|
| `ToastTest.Attribution` | Orchestrates issue production by combining test failures, crash analysis, and sanitizer reports into SuiteResult issues. |
| `ToastTest.Attribution.TimeWindows` | Time window calculations for attributing diagnostics to tests. |
| `ToastTest.Attribution.ServerLogs` | Extract server log data for attribution context. |
| `ToastTest.ArtifactCollector` | Inventories filesystem artifacts (coredumps, sanitizer logs) for server instances. Pure discovery only. |
| `ToastTest.Enrichment.Logs` | Extract excerpts from ArangoDB log files. Supports time-windowed and crash-line extraction. |
| `ToastTest.Enrichment.Sanitizer` | Read and classify sanitizer report files. |
| `ToastTest.Enrichment.Coredump` | Enrichment wrapper around Toast.Diagnostics.Coredump.analyze, transforms Report to SuiteResult issue format. |

### Formatting (`ToastTest.Formatting.*`)

| Module | Role |
|--------|------|
| `ToastTest.Formatting` | Base module with shared formatting utilities (colorize, etc.). Also provides `display_module_name/1` which converts module atoms to display strings (strips `Elixir.` prefix for Elixir modules, returns raw path for JS file path atoms). |
| `ToastTest.Formatting.CLI` | Google Test-style console output with timestamps. Replaces ExUnit.CLIFormatter. |
| `ToastTest.Formatting.Issues` | Format diagnostic issues for display. |
| `ToastTest.Formatting.Logs` | Format server log excerpts for display. |
| `ToastTest.Formatting.PostExecSummary` | Post-execution summary (crash attribution, sanitizer errors, coredump traces). |
| `ToastTest.Formatting.RunSummary` | Overall run summary across all suites. |
| `ToastTest.Formatting.RrSummary` | Summary of rr recording locations. |

### Client (`Toast.Client.*`)

| Module | Role |
|--------|------|
| `Toast.Client` | Thin REST client (wraps Req). Supports database scoping, auth (basic, JWT), API versioning, HTTP/2. |
| `Toast.Client.Admin` | `/_admin/*` endpoints (version, server status, shutdown, statistics). |
| `Toast.Client.AQL` | AQL query execution (`/_api/cursor`). |
| `Toast.Client.Collection` | Collection CRUD (`/_api/collection`). |
| `Toast.Client.Document` | Document CRUD (`/_api/document`). |
| `Toast.Client.Index` | Index management (`/_api/index`). |
| `Toast.Client.Database` | Database CRUD (`/_api/database`). |
| `Toast.Client.Graph` | Named graph lifecycle via Gharial API (`/_api/gharial`). |
| `Toast.Client.Vertex` | Vertex operations via Gharial API. |
| `Toast.Client.Edge` | Edge operations via Gharial API. |
| `Toast.Client.View` | ArangoSearch view management (`/_api/view`). |
| `Toast.Client.Analyzer` | Analyzer management (`/_api/analyzer`). |
| `Toast.Client.User` | User management (`/_api/user`). |
| `Toast.Client.Transaction` | Stream transaction lifecycle (`/_api/transaction`). |
| `Toast.Client.Utils` | Client utility functions. |

### Results (`ToastTest.*`)

| Module | Role |
|--------|------|
| `ToastTest.ResultCollector` | ExUnit formatter that collects test results with module-level timestamp tracking. |
| `ToastTest.ResultCollector.State` | Internal state struct for ResultCollector. |
| `ToastTest.SuiteResult` | Data struct for structured suite results with typed issues, module results, and test results. |
| `ToastTest.SuiteResult.JSON` | JSON serialization of SuiteResult. |
| `ToastTest.SuiteResult.JUnitXML` | JUnit XML serialization of SuiteResult for CI. |
| `ToastTest.DiagnosticsSummary` | Aggregate diagnostics across suites: sanitizer detection, artifact inventory, exit code computation. |
| `ToastTest.ResultPackaging` | Tiered CI artifact packaging gated on outcome (Tier 1: always, Tier 2: any failure, Tier 3: server crash). |

### Utilities

| Module | Role |
|--------|------|
| `Toast.Application` | OTP application callback. Starts supervision tree, configures logging. |
| `Toast.System` | System resource detection (memory via cgroup v2 or `/proc/meminfo`). |
| `Toast.PortAllocator` | GenServer for sequential port allocation with socket probing. |
| `Toast.Utils` | Utility functions (conditional_put, compact, compact_join). |
| `Toast.Utils.Filesystem` | Server directory creation, arangod binary discovery, repo root detection. |
| `Toast.Utils.Compression` | Compression tool detection and wrappers (zstd, gzip). |
| `Toast.Utils.Polling` | Generic poll-until-condition utility with deadline-based timeout. |
| `Toast.LogFormatter` | Custom log format for both Elixir Logger and Erlang `:logger` handler. |
| `ToastTest.LogAnalysis` | Data transformation for server log analysis (parsing, filtering, k-way merge). Used by `mix toast.analyze`. |
| `ToastTest.Supervisor` | Supervisor for ToastTest-specific processes. |

### CLI (`Mix.Tasks.*`)

| Module | Role |
|--------|------|
| `Mix.Tasks.Toast` | Main entry point: suite discovery, compilation, runner invocation. |
| `Mix.Tasks.Toast.Helpers` | Pure helper functions for CLI arg parsing and option mapping. |
| `Mix.Tasks.Toast.Gen.Suite` | Code generator for new test suites. |
| `Mix.Tasks.Toast.Analyze` | Main entry point for offline result analysis. Dispatches to subcommands. |
| `Mix.Tasks.Toast.Analyze.Issues` | List all issues across suites (default subcommand). |
| `Mix.Tasks.Toast.Analyze.Detail` | Show full diagnostic detail with logs, backtraces, and disassembly. |
| `Mix.Tasks.Toast.Analyze.Info` | Overview of diagnostics file contents. |
| `Mix.Tasks.Toast.Analyze.Perf` | Performance analysis (module/test timing breakdown). |
| `Mix.Tasks.Toast.Analyze.Data` | Data loading and processing for analysis subcommands. |
| `Mix.Tasks.Toast.Analyze.Weights` | Suggest module weights based on runtime distribution from results. |


## Deployment Subsystem

### Controller Architecture

The Controller is a GenServer that manages the complete lifecycle of a
deployment. It delegates startup to `DeployPipeline` and shutdown to
`ShutdownPipeline`, while directly handling server control operations and
crash classification.

```
Toast.Deployment (public API facade)
  |
  v
Toast.Deployment.Controller (GenServer)
  |
  |  delegates to:
  |
  +-- DeployPipeline   -- multi-phase startup (start processes, launch by role, health check)
  +-- ShutdownPipeline -- ordered shutdown, rollback, abort
  +-- ServerLifecycle  -- pure functions for stop/kill/pause/resume/relaunch
  +-- Factory          -- builds launch specs from Config
  +-- Health           -- HTTP health check polling
```

The Controller owns all shared logic:
- GenServer lifecycle and message routing
- Server control operations (stop/kill/pause/resume/restart/start)
- Crash handling protocol (expected vs unexpected crashes)
- Target resolution (string, role, arango_id)
- Health monitor management

### Controller.State

```elixir
%Controller.State{
  config: %Toast.Deployment.Config{},  # Deployment configuration
  id: "single-00",                     # Deployment ID
  status: :ready,                      # Current deployment status
  servers: %{                          # Map of server_id => ServerInstance
    "single-00" => %ServerInstance{...}
  },
  expected_crashes: %{},               # Map of server_id => CrashExpectation
  error: nil,                          # Error details if status is :failed
  agency_dump: nil,                    # Captured agency state (cluster only)
  event_listener: ToastTest.DeploymentListener,  # EventListener behaviour impl
  jwt_provider: %Toast.JWT.Provider{}  # JWT signer (nil when auth disabled)
}
```

### Deployment Status State Machine

```
                         +----------+
                         | :stopped |
                         +----+-----+
                              |
                        deploy(timeout)
                              |
                         +----v-----+
                         | :starting|
                         +----+-----+
                              |
                   +----------+----------+
                   |                     |
              (success)             (failure)
                   |                     |
              +----v---+           +-----v----+
              | :ready |           | :failed  |
              +----+---+           +-----+----+
                   |                     |
        server     |              shutdown(timeout)
        control    |                     |
        ops        |                +----v-----+
                   |                | :stopped |
              +----v------+        +----------+
              | :degraded |
              +----+------+
                   |
            restore server
                   |
              +----v---+
              | :ready |
              +--------+

    Any state with running servers:
      unexpected crash --> :failed
      server_unhealthy --> :failed

    :ready or :degraded:
      shutdown(timeout) --> :stopping --> :stopped
    :failed:
      shutdown(timeout) --> :stopped
```

**:degraded** means some servers are intentionally down (stopped, killed, or
paused by test code). Tests that stop servers for chaos testing must restore
them before finishing. The Runner checks deployment health between tests and
aborts if the deployment is still degraded.

### Deploy Sequence

`DeployPipeline.run/4` handles both single-server and cluster deployments
through a unified role-group-based sequence:

```
1. Factory.build_single_server() or Factory.build_cluster()
   --> creates server dirs, builds arangod CLI args
   --> returns list of launch specs with role annotations

2. Start all ServerProcess GenServers (but don't launch OS processes yet)

3. Launch servers by role group in dependency order:
   [single] or [agent -> dbserver -> coordinator]

   For each role group:
     a. Launch OS processes in parallel (ServerProcess.launch)
     b. Wait for health (role-specific):
        - agents: wait for agency consensus (/_api/agency/config)
        - others: poll /_api/version until 200

4. Start health monitors for all servers

5. Post-deploy:
   - Cluster: fetch cluster ID mapping via /_admin/cluster/health
     (maps toast server IDs to ArangoDB-internal IDs)
```

The phased startup is required because ArangoDB cluster nodes depend on the
agency being available before they can start. DBservers and coordinators are
launched only after agency consensus is confirmed.

On failure at any step, `ShutdownPipeline.rollback/2` stops all processes
and health monitors, sets status to `:failed`.

### Shutdown Sequence

`ShutdownPipeline.shutdown/2` stops servers in reverse dependency order:

1. Stop all health monitors
2. Stop servers by role in reverse order:
   - Cluster: coordinators -> dbservers -> agents (parallel within each group)
   - Single: stop the single server
3. Collect per-server diagnostics

### Crash Handling Protocol

When a server process exits unexpectedly, erlexec sends a `{:DOWN, os_pid, ...}`
message to ServerProcess, which translates it to `{:server_crashed, server_id, crash_info}`
sent to the Controller (its listener).

The Controller classifies the crash:

```
  {:server_crashed, server_id, crash_info}
        |
        v
  Is server_id in expected_crashes?
  +-- YES --> :expected (test called expect_crash beforehand)
  |           Update expected_crashes with crash_info.
  |           Wake any waiter (verify_crash).
  |
  +-- NO --> Is server.expecting_exit == true?
             +-- YES --> Was signal in [nil, 15]?
             |           +-- YES --> :intentional_exit (clean SIGTERM from stop_server)
             |           +-- NO  --> :crash_during_intentional_stop
             |                       (server crashed with unexpected signal during stop)
             |                       Set status -> :failed
             |
             +-- NO --> :unexpected_crash
                        Set status -> :failed
                        Notify event_listener.on_crash
```

The `on_crash` callback (via `ToastTest.DeploymentListener`) triggers
`ToastTest.CrashMonitor.handle_crash/2` which calls `Runner.abort!` to stop
further test execution. Events are recorded in `ToastTest.EventStore` for
post-run analysis.

### Server Control Operations

Tests can manipulate deployment servers for chaos testing:

| Operation | Effect | ServerProcess action | Health monitor |
|-----------|--------|---------------------|----------------|
| `stop_server` | Graceful SIGTERM | `ServerProcess.stop` | Suspended |
| `kill_server` | Immediate SIGKILL | `ServerProcess.kill` | Suspended |
| `pause_server` | SIGSTOP (freeze) | `ServerProcess.pause` | Suspended |
| `resume_server` | SIGCONT (unfreeze) | `ServerProcess.resume` | Resumed |
| `restart_server` | Stop + relaunch | Stop then `relaunch_and_wait` | Resumed |
| `start_server` | Relaunch stopped/crashed | `relaunch_and_wait` | Resumed |

Health monitors are suspended during intentional operations to prevent false
unhealthy notifications. The `expecting_exit` flag on ServerInstance prevents the
Controller from treating the subsequent process exit as a crash.

### Target Resolution

Server control operations accept flexible targets:

- **String**: direct server ID lookup
- **`[role: :coordinator]`**: all servers with that role
- **`[role: :dbserver, index: 0]`**: specific server by role + index
- **`[arango_id: "PRMR-abc123"]`**: by ArangoDB-assigned internal ID

The Controller resolves targets to concrete server IDs, then applies the
operation to each resolved server.


## Process Management

### ServerProcess

`Toast.Process.ServerProcess` is a GenServer that wraps erlexec for a single
OS process. It is intentionally NOT a restart-on-crash process.

```
State machine:

  :stopped  --launch-->  :running  --stop-->  :stopping  --exit-->  :stopped
                             |                    |
                         crash/kill           timeout
                             |                    |
                             v                    v
                         :crashed           SIGKILL --> :stopped
                             |
                         relaunch
                             |
                             v
                         :running
```

Key design decisions:

- **erlexec** (`{:group, 0}, :kill_group`): places the server in its own
  process group and kills the entire group on BEAM exit. This prevents orphan
  arangod processes if the test framework crashes.

- **Monitor, not link**: erlexec's `:monitor` option sends `{:DOWN, os_pid, ...}`
  messages instead of linking. This lets the GenServer handle exits without
  itself being killed.

- **Two-phase stop**: `do_stop` sends SIGTERM via `exec.stop`, sets a timer.
  If the process doesn't exit before timeout, sends SIGKILL. If even that
  doesn't work (kill_timeout), gives up and replies `:ok` to avoid hanging.

- **stderr capture**: when `output_handler` is provided, erlexec pipes stderr
  and forwards chunks via `{:stderr, os_pid, data}` messages. The handler
  (typically `ServerLifecycle.print_server_output/2`) prints each line with a
  server ID prefix. Stdout is connected to `/dev/null` via `{:stdin, :null}`.

### HealthMonitor

`Toast.Process.HealthMonitor` complements crash detection:

- erlexec detects **process death** (crashes, kills)
- HealthMonitor detects **liveness failures** (deadlocks, infinite loops,
  resource exhaustion) where the process is still alive but not serving

It polls `/_api/version` every 1 second. After 3 consecutive failures, it sends
`{:server_unhealthy, server_id}` to the Controller, which then kills the
unresponsive server and sets status to `:failed`.

The monitor supports `:suspend` / `:resume` messages for intentional server
control operations, preventing false positives when a test deliberately stops
or pauses a server.


## Configuration

Configuration is resolved by `Toast.Env` from three sources with clear precedence:

```
keyword opts (code)  >  env vars (TOAST_*)  >  .toast.local.exs  >  defaults
```

The `.toast.local.exs` file is skipped when `TOAST_CI=true` (CI environments
should use env vars exclusively).

`Toast.Env.load/1` resolves all configuration into a map and writes it to
Application env. Two config structs then read from Application env:

- **`Toast.Deployment.Config`** -- deployment infrastructure concerns (build dir,
  server args, sanitizers, cluster topology, JWT, SSL, startup/shutdown timeouts)
- **`ToastTest.Config`** -- test execution concerns (result dir, diagnostics,
  CI settings, debugger, test/global timeouts)

This split reflects the layering: `Toast` is reusable infrastructure;
`ToastTest` is the test runner built on top. In an interactive session, only
`Toast.Deployment.Config` is needed.

Key configuration groups:

| Group | Fields |
|-------|--------|
| Paths | `build_dir`, `base_dir`, `result_dir`, `coredump_dir` |
| Deployment | `deployment_mode`, cluster topology, `server_args` (per-role maps), `authentication`, `jwt_algorithm`, `ssl`, `protocol` |
| Timeouts | `global_timeout`, `test_timeout`, `startup_timeout`, `shutdown_timeout`, `coredump_timeout`, `timeout_factor` |
| Sanitizer | `sanitizer_override`, `active_sanitizers` (MapSet of env var names) |
| Display | `show_server_logs` |
| Debug | `debugger` (:gdb, :lldb, :auto, :none), `dump_agency_on_error`, `attach_debugger` |
| Recording | `rr` (MapSet of roles to record), `rr_path` |
| Resources | `memory_budget` |
| CI | `ci`, `keep_data` |

`timeout_factor` multiplies all timeout values. It defaults to 3 when any
sanitizer is detected and 10 when rr recording is enabled, since both run
significantly slower.


## Client Subsystem

`Toast.Client` wraps `Req` with ArangoDB-specific features:

```elixir
client = Toast.Client.new("http://127.0.0.1:8529",
  database: "mydb",
  api_version: 1,
  auth: {:basic, "root", ""},
  protocol: :http2
)
```

URL construction: `/{version_prefix}/{db_prefix}{path}`
- version_prefix: `/_arango/v1` (integer) or `/_arango/edge` (string)
- db_prefix: `/_db/mydb`

Convenience wrappers:
- `unwrap/2`: extract body from `{:ok, %{status: 200, body: body}}`
- `unwrap_ok/2`: just check status, return `:ok` or `{:error, ...}`

Domain-specific modules (`Client.Collection`, `Client.Document`, etc.) build
on the base client with typed parameters and response handling.

Authentication modes:
- `{:basic, user, pass}` -- HTTP Basic auth
- `{:jwt, token}` -- Static JWT bearer token
- `{:jwt_provider, provider}` -- Dynamic JWT from a `Toast.JWT.Provider`


## Event System

Toast uses an event-driven architecture for deployment lifecycle tracking:

```
Toast.Deployment.Events      -- emits events (server_started, server_stopped, etc.)
       |
       v
Toast.Deployment.EventListener  -- behaviour (on_event/1, on_crash/2)
       |
       +-- DefaultEventListener  -- no-op (used in interactive mode)
       +-- ToastTest.DeploymentListener  -- records to EventStore + triggers CrashMonitor
              |
              +-- ToastTest.EventStore  -- ETS-backed chronological event store
              +-- ToastTest.CrashMonitor  -- calls Runner.abort! on unexpected crash
```

Events are maps with `:event`, `:deployment_id`, `:timestamp` keys plus
event-specific data. `EventStore.Projections` provides derived views
(e.g., `pids_by_server/0` for mapping server IDs to OS process IDs).


## CLI (Mix Tasks)

### `mix toast`

Entry point for suite-based test execution:

```
mix toast [suite_name...] [suite_name/test_file.exs[:line]] [options]
```

Suite discovery:
1. Find `suites/*/suite.ex` files
2. Filter to requested suites (or all if none specified)
3. Compile suite modules, identify `ToastTest.Suite` behaviours
3a. For JS suites (modules using `ToastTest.JavascriptSuite`):
    discover JS files from configured paths, generate one Elixir module per JS file
    via ModuleBuilder, apply filename-based tag inference
4. For each suite: discover helpers (.ex) and test files (test_*.exs)
5. Compile helpers, require test files
6. Filter test modules to those using the suite (`__toast_suite__/0`)

The Runner then receives `[{suite_module, test_modules, suite_opts, suite_name}]`
where test_modules may include both compiled Elixir modules and dynamically
generated JS modules.

### `mix toast.gen.suite`

Generates boilerplate:
```
suites/my_suite/
  suite.ex         -- defmodule MySuite.Suite do use ToastTest.Suite end
  test_example.exs -- example test using the suite
```

### `mix toast.analyze`

Offline analysis of test results from `.diagnostics.etf` files:

```
mix toast.analyze [subcommand] [RESULT_DIR] [options]
```

Subcommands:
- `issues` (default) -- list all issues across suites
- `detail` -- show full diagnostic detail (logs, backtraces, disassembly)
- `info` -- overview of diagnostics file contents
- `perf` -- performance analysis (module/test timing breakdown)
- `weights` -- suggest module weights based on runtime distribution

The detail subcommand accepts issue specs (`3`, `2-4`, `all`, `crashes`,
`sanitizer`) and supports log filtering (`--logs`, `--log-window`,
`--log-min-level`) and backtrace options (`--threads`, `--backtrace-frames`,
`--disassembly`).
