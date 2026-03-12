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
  | Config      |    | ToastTest.Runner |    | ResultPackaging  |
  +-------------+    +------------------+    +------------------+
                              |
              +---------------+---------------+
              |               |               |
              v               v               v
  +-----------------+  +--------------+  +------------------------+
  | Deployment      |  | Attribution  |  | Diagnostics            |
  | (Controller,    |  | & Enrichment |  | (Coredump, Sanitizer,  |
  |  Lifecycle,     |  | (Attribution,|  |  AgencyDump)           |
  |  Factory,       |  |  Logs,       |  +------------------------+
  |  Health)        |  |  Sanitizer,  |           ^
  +-----------------+  |  Coredump,   |           |
              |        |  PostExec    |     (uses low-level
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

1. CLI parses args, loads config, discovers suites
2. Runner starts a deployment per suite via `Toast.Deployment.start/2`
3. Controller (GenServer) orchestrates server launch via Factory + ServerProcess
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
            Delegates to mode callback modules (SingleServer or Cluster).
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

### Deployment (`Toast.Deployment.*`)

| Module | Role |
|--------|------|
| `Toast.Deployment` | Public API facade. Start/stop deployments, query status, server control ops. |
| `Toast.Deployment.Controller` | GenServer + behaviour. Shared state machine. Delegates to mode callbacks. |
| `Controller.SingleServer` | Mode callback for single arangod instance. |
| `Controller.Cluster` | Mode callback for agent/dbserver/coordinator cluster. |
| `Controller.Helpers` | Helper functions for Controller (server lookup, update, health monitor management, relaunch logic). |
| `Toast.Deployment.ServerLifecycle` | Pure functions for server ops (stop, kill, pause, resume, relaunch, crash handling). Used by Controller. |
| `Toast.Deployment.Factory` | Builds launch specs (executable path, args, env, dirs) from Config. |
| `Toast.Deployment.CommandBuilder` | Generates arangod CLI arguments per role. |
| `Toast.Deployment.Health` | HTTP health check polling (`/_api/version`, `/_api/agency/config`). |
| `Toast.Deployment.FailurePoint` | Failure injection via `/_admin/debug/failat` REST endpoints. |
| `Toast.Deployment.ServerInstance` | Data struct: runtime state of one server (ports, pids, endpoints, state). |
| `Toast.Deployment.Supervisor` | DynamicSupervisor for Controller processes. |

### Process Management (`Toast.Process.*`)

| Module | Role |
|--------|------|
| `Toast.Process.ServerProcess` | GenServer wrapping erlexec for one OS process. Lifecycle + crash detection. |
| `Toast.Process.HealthMonitor` | GenServer polling HTTP health. Notifies listener after N consecutive failures. |
| `Toast.Process.CrashInfo` | Data struct for crash information (exit status, signal, timestamp, PID). |
| `Toast.Process.CrashEvent` | Data struct for crash event metadata. |
| `Toast.Process.Supervisor` | DynamicSupervisor for ServerProcess and HealthMonitor children. |

### Test Execution (`ToastTest.*`)

| Module | Role |
|--------|------|
| `ToastTest.Runner` | Suite-based test runner (derived from ExUnit.Runner). Orchestrates deploy -> run -> collect. |
| `ToastTest.Suite` | Behaviour + `__using__` macro for suite definition. Defines `deployment_config/0`. |
| `ToastTest.Case` | ExUnit.CaseTemplate providing `deployment`, `endpoint`, `client` context. |
| `ToastTest.ExUnitCompat` | Shim over ExUnit internal APIs (EventManager, RunnerStats). |
| `ToastTest.SuiteRun` | Data struct: per-suite execution context (module, deployment, deadline). |
| `ToastTest.DeploymentRegistry` | ETS-based mapping of suite modules to active deployments. |
| `ToastTest.CrashMonitor` | Default `on_crash` callback; calls `Runner.abort!`. |
| `ToastTest.Abort` | ETS-backed suite-level abort state with human-readable messages. |
| `ToastTest.ProcessHistory` | GenServer recording server lifecycle events (start/stop/crash). |
| `ToastTest.StateCleanup` | Resets shared state between suite runs (ETS tables, formatters, callbacks). |
| `ToastTest.TestLifecycle` | Shared test lifecycle primitives (spawn_setup_all, spawn_test, etc.) used by both Interactive and Runner. |
| `ToastTest.Interactive` | Run individual tests against an existing deployment (debugging). |

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
| `ToastTest.ArtifactCollector` | Inventories filesystem artifacts (coredumps, sanitizer logs) for server instances. Pure discovery only. |
| `ToastTest.Enrichment.Logs` | Extract excerpts from ArangoDB log files. Supports time-windowed and crash-line extraction. |
| `ToastTest.Enrichment.Sanitizer` | Read and classify sanitizer report files. |
| `ToastTest.Enrichment.Coredump` | Enrichment wrapper around Toast.Diagnostics.Coredump.analyze, transforms Report to SuiteResult issue format. |
| `ToastTest.PostExecSummary` | Post-execution summary formatting for CLI output (crash attribution, sanitizer errors, coredump traces). |

### Client (`Toast.Client.*`)

| Module | Role |
|--------|------|
| `Toast.Client` | Thin REST client (wraps Req). Supports database scoping, auth, API versioning. |
| `Toast.Client.Admin` | `/_admin/*` endpoints (version, server status, shutdown). |
| `Toast.Client.AQL` | AQL query execution (`/_api/cursor`). |
| `Toast.Client.Collection` | Collection CRUD (`/_api/collection`). |
| `Toast.Client.Document` | Document CRUD (`/_api/document`). |
| `Toast.Client.Index` | Index management (`/_api/index`). |

### Results (`ToastTest.*`)

| Module | Role |
|--------|------|
| `ToastTest.CLIFormatter` | Google Test-style console output with timestamps. Replaces ExUnit.CLIFormatter. |
| `ToastTest.ResultCollector` | ExUnit formatter that collects test results with module-level timestamp tracking. |
| `ToastTest.SuiteResult` | Data struct for structured suite results with typed issues, module results, and test results. |
| `ToastTest.SuiteResult.JSON` | JSON serialization of SuiteResult. |
| `ToastTest.SuiteResult.JUnitXML` | JUnit XML serialization of SuiteResult for CI. |
| `Toast.ResultPackaging` | Tiered CI artifact packaging (Tier 1: always, Tier 2: logs archive, Tier 3: coredumps). |

### Utilities

| Module | Role |
|--------|------|
| `Toast.Config` | Load configuration from env vars, CLI opts, `.toast.local.exs`. |
| `Toast.PortAllocator` | GenServer for sequential port allocation with socket probing. |
| `Toast.Utils` | Utility functions (conditional_put, compact, compact_join). |
| `Toast.Utils.Filesystem` | Server directory creation, arangod binary discovery, repo root detection. |
| `Toast.LogFormatter` | Custom log format for both Elixir Logger and Erlang `:logger` handler. |

### CLI (`Mix.Tasks.*`)

| Module | Role |
|--------|------|
| `Mix.Tasks.Toast` | Main entry point: suite discovery, compilation, runner invocation. |
| `Mix.Tasks.Toast.Helpers` | Pure helper functions for CLI arg parsing and option mapping. |
| `Mix.Tasks.Toast.Gen.Suite` | Code generator for new test suites. |


## Deployment Subsystem

### Controller + Mode Callback Architecture

The Controller is a GenServer that provides a shared state machine and
delegates mode-specific logic to callback modules. This is a behaviour-based
strategy pattern rather than inheritance:

```
Toast.Deployment.Controller (GenServer)
  |
  |  defines behaviour callbacks:
  |    init_mode_state/0
  |    init_servers/1
  |    deploy/2
  |    shutdown/2
  |    derive_status/1
  |    resolve_target/2
  |    build_info/1
  |    handle_call_extra/3  (optional)
  |
  +-- Controller.SingleServer (implements behaviour)
  |     Single arangod instance.
  |
  +-- Controller.Cluster (implements behaviour)
        Full cluster: agents + dbservers + coordinators.
```

The Controller owns all shared logic:
- GenServer lifecycle and message routing
- Server control operations (stop/kill/pause/resume/restart/start)
- Crash handling protocol (expected vs unexpected crashes)
- Health monitor management
- Target resolution dispatch

Mode modules own deployment-specific logic:
- Which servers to create and in what order
- Deploy sequence (cluster requires phased startup)
- Shutdown order (cluster stops coordinators first, then dbservers, then agents)
- Status derivation from server states
- Target resolution (cluster supports role-based and cluster-id targeting)

**Why this design**: A single GenServer with mode callbacks avoids the
complexity of two separate GenServer implementations that would duplicate the
entire control operation and crash handling protocol. The mode-specific logic
is genuinely different (phased vs single startup), but the framework around it
(state management, message routing, health monitoring) is identical.

### Controller.State

```elixir
%Controller.State{
  config: %Config{},          # Framework configuration
  id: "toast-42",             # Deployment ID
  mode: Controller.SingleServer,  # Mode callback module
  mode_state: %{},            # Mode-specific state (cluster keeps agent/dbserver/coordinator lists)
  status: :ready,             # Current deployment status
  servers: %{                 # Map of server_id => ServerInstance
    "toast-42" => %ServerInstance{...}
  },
  expected_crashes: %{},      # Map of server_id => %{timer, crash_info}
  error: nil,                 # Error details if status is :failed
  diagnostics: nil,           # Collected after shutdown
  on_crash: &fun/2,           # Crash notification callback
  on_event: &fun/1            # Lifecycle event callback
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

### Deploy Sequence: Single Server

```
1. PortAllocator.allocate()
2. Factory.build_single_server(config, id, port)
   --> creates server dirs, builds arangod CLI args
3. Process.Supervisor.start_server(launch_spec)
   --> starts ServerProcess GenServer
4. ServerProcess.launch()
   --> erlexec starts the OS process
5. Health.wait_until_ready(endpoint, timeout)
   --> polls /_api/version until 200
6. Controller.start_single_health_monitor()
   --> starts HealthMonitor for continuous checking
7. status -> :ready
```

On failure at any step, `rollback` stops all processes and health monitors,
sets status to `:failed`.

### Deploy Sequence: Cluster

```
1. Factory.build_cluster(config, id)
   --> allocates ports for all nodes, creates dirs, builds args
   --> returns topology: %{agents: [...], dbservers: [...], coordinators: [...]}

2. Start all ServerProcess GenServers (but don't launch OS processes yet)

3. Launch agents (parallel via Task.async_stream)
   --> ServerProcess.launch() for each agent
   --> No health check yet (agency needs consensus first)

4. Wait for agency consensus
   --> Health.wait_for_agency_ready(agent_endpoints)
   --> Polls /_api/agency/config on all agents
   --> Checks: all responding, all have same leaderId, lastAcked exists

5. Launch dbservers (parallel, with health check)
   --> ServerProcess.launch() + Health.wait_until_ready()

6. Launch coordinators (parallel, with health check)
   --> ServerProcess.launch() + Health.wait_until_ready()

7. Start health monitors for all servers

8. Fetch cluster ID mapping
   --> GET coordinator/_admin/cluster/health
   --> Maps toast server IDs to cluster-internal IDs

9. status -> :ready
```

The phased startup is required because ArangoDB cluster nodes depend on the
agency being available before they can start. DBservers and coordinators are
launched only after agency consensus is confirmed.

### Shutdown Sequence

**Single server**: stop health monitor, stop ServerProcess, collect diagnostics.

**Cluster**: stop in reverse dependency order:
1. Stop all health monitors
2. Stop coordinators (parallel)
3. Stop dbservers (parallel)
4. Stop agents (parallel)
5. Collect per-server diagnostics

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
  |           Set status based on derive_status().
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
                        Notify on_crash callback
```

The `on_crash` callback (typically `CrashMonitor.handle_crash/2`) calls
`Runner.abort!` to stop further test execution. The `on_event` callback
records the event in ProcessHistory for post-run analysis.

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
- **`[role: :coordinator]`**: all servers with that role (cluster)
- **`[role: :dbserver, index: 0]`**: specific server by role + index
- **`[cluster_id: "PRMR-abc123"]`**: by ArangoDB cluster-internal ID

The Controller dispatches to `mode.resolve_target(state, target)` which returns
`{:ok, [server_id_list]}` or `{:error, reason}`. Operations are then applied
to each resolved server via `apply_to_each`.


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

`Toast.Config` loads from three sources with clear precedence:

```
keyword opts (code)  >  env vars (TOAST_*)  >  .toast.local.exs  >  defaults
```

The `.toast.local.exs` file is skipped when `TOAST_CI=true` (CI environments
should use env vars exclusively).

Key configuration groups:

| Group | Fields |
|-------|--------|
| Paths | `build_dir`, `work_dir`, `result_dir` |
| Deployment | `deployment_mode`, `cluster_agents`, `cluster_dbservers`, `cluster_coordinators`, `cluster_replication_factor` |
| Timeouts | `global_timeout`, `test_timeout`, `startup_timeout`, `shutdown_timeout`, `coredump_timeout`, `timeout_factor` |
| Sanitizer | `explicit_sanitizer`, `sanitizer` (MapSet of active env var names) |
| Display | `show_server_logs` |
| Debug | `debugger` (:gdb, :lldb, :auto, :none), `dump_agency_on_error` |
| CI | `ci`, `keep_work_dir` |

`timeout_factor` multiplies all timeout values. It defaults to 3 when any
sanitizer is detected, since sanitized builds run significantly slower.


## Client Subsystem

`Toast.Client` wraps `Req` with ArangoDB-specific features:

```elixir
client = Toast.Client.new("http://127.0.0.1:8529",
  database: "mydb",
  api_version: 1,
  auth: {:basic, "root", ""}
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
4. For each suite: discover helpers (.ex) and test files (test_*.exs)
5. Compile helpers, require test files
6. Filter test modules to those using the suite (`__toast_suite__/0`)

The Runner then receives `[{suite_module, test_modules, suite_opts}]`.

### `mix toast.gen.suite`

Generates boilerplate:
```
suites/my_suite/
  suite.ex         -- defmodule MySuite.Suite do use ToastTest.Suite end
  test_example.exs -- example test using the suite
```

