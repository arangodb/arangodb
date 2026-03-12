# Data Structures and Dependencies

Key data structures and module dependency map for the TOAST test framework.
See [architecture.md](./architecture.md) for the system overview.


## Key Data Structures

### Toast.Deployment.t()

The public handle returned by `Toast.Deployment.start/2`. Passed to test
context and all deployment operations. Contains enough to address the
Controller GenServer but not the full internal state.

```elixir
%Toast.Deployment{
  id: "toast-42",                     # unique deployment identifier
  mode: :single_server,               # :single_server | :cluster
  config: %Toast.Config{...},         # frozen config snapshot
  controller: #PID<0.123.0>,          # Controller GenServer pid
  endpoint: "http://127.0.0.1:8529",  # primary endpoint (coordinator for cluster)
  work_dir: "/tmp/toast/run_42"       # server data directory
}
```

Why this exists as a separate struct from Controller.State: the Controller.State
is internal to the GenServer and contains mutable runtime state (server pids,
health monitors, crash tracking). The Deployment struct is a stable reference
that test code can hold without coupling to GenServer internals.


### Toast.Deployment.Controller.State

Internal state of the Controller GenServer. Not exposed outside the GenServer
process. See [architecture.md](./architecture.md#controllerstate) for the full
breakdown.


### Toast.Deployment.ServerInstance.t()

Runtime state of a single server process within a deployment:

```elixir
%Toast.Deployment.ServerInstance{
  id: "toast-42",                # server identifier (same as deployment ID for single)
  role: :single,                 # :single | :agent | :dbserver | :coordinator
  port: 8529,                   # TCP port for HTTP API
  endpoint: "http://127.0.0.1:8529",
  pid: 12345,                   # OS process ID (from erlexec)
  log_file: "/tmp/toast/.../arangod.log",
  server_dir: "/tmp/toast/.../",
  server_pid: #PID<0.456.0>,    # ServerProcess GenServer pid
  health_monitor: #PID<0.789.0>,# HealthMonitor GenServer pid
  operational_state: :running,   # :running | :paused | :stopped | :killed | :crashed
  intentional: false,            # true when state change was requested by test code
  launch_spec: %{...}           # executable, args, env, dirs -- reused for relaunch
}
```

The `intentional` flag is the key mechanism for distinguishing test-driven
server manipulation from actual failures. When a test calls `stop_server`,
`intentional` is set to `true` before the stop. When the process exit
notification arrives, the Controller checks this flag to decide whether to
trigger the crash protocol.

`operational_state` and `intentional` together determine the Controller's
response:

```
operational_state  intentional  Meaning
-----------------  -----------  -------
:running           false        Normal operation
:paused            true         Test paused the server (SIGSTOP)
:stopped           true         Test stopped the server (SIGTERM)
:killed            true         Test killed the server (SIGKILL)
:crashed           true         Expected crash (test called expect_crash)
:crashed           false        Unexpected crash --> :failed status
```


### Toast.Config.t()

Framework configuration. Immutable after `Config.load/1`. All timeouts are
pre-multiplied by `timeout_factor` (3x for sanitizer builds).

```elixir
%Toast.Config{
  # Paths
  build_dir: "/path/to/build",       # where arangod binary lives
  work_dir: "/tmp/toast/run_42",     # per-run server data
  result_dir: "toast-results",       # where results.json etc. are written

  # Deployment
  deployment_mode: :single_server,   # default mode
  cluster_agents: 3,
  cluster_dbservers: 3,
  cluster_coordinators: 1,
  cluster_replication_factor: 2,

  # Timeouts (all in milliseconds, pre-multiplied by timeout_factor)
  global_timeout: 3_600_000,         # entire test run
  test_timeout: 300_000,             # single test
  startup_timeout: 60_000,           # deployment startup
  shutdown_timeout: 60_000,          # deployment shutdown
  coredump_timeout: 120_000,         # coredump analysis budget
  timeout_factor: 1,                 # multiplier (3 for sanitizer)

  # Sanitizer
  explicit_sanitizer: "alubsan",     # forced sanitizer type, or nil
  sanitizer: #MapSet<["ASAN_OPTIONS", ...]>,  # active env var names

  # Other
  show_server_logs: false,           # print arangod stderr
  keep_work_dir: false,              # preserve server data after run
  api_version: nil,                  # API version prefix for URLs
  debugger: :auto,                   # :gdb | :lldb | :auto | :none
  dump_agency_on_error: true,        # capture agency state on failure
  ci: false                          # enable CI packaging
}
```


### Toast.Client.t()

REST client wrapping `Req`. Supports database scoping and API versioning:

```elixir
%Toast.Client{
  base_url: "http://127.0.0.1:8529",
  database: "mydb",                  # nil for system-level requests
  api_version: 1,                    # integer, string, or nil
  auth: {:basic, "root", ""},        # {:basic, u, p} | {:jwt, token} | nil
  req: %Req.Request{...}             # underlying HTTP client
}
```

URL construction: `{base_url}/{version_prefix}/{db_prefix}{path}`
- version_prefix: `/_arango/v1` (integer) or `/_arango/edge` (string)
- db_prefix: `/_db/mydb`
- Both are omitted when nil


### Toast.Diagnostics.Coredump.Report.t()

Structured result from debugger analysis of a core dump:

```elixir
%Toast.Diagnostics.Coredump.Report{
  core_path: "/tmp/toast/.../core.12345",
  binary_path: "/path/to/build/bin/arangod",
  debugger: :lldb,                   # which debugger produced this
  signal: "SIGSEGV",                 # signal name from debugger output
  faulting_address: "0x0",           # address that caused the fault
  crash_thread: 3,                   # thread index that crashed
  threads: [                         # all threads with backtraces
    %{
      index: 0,
      name: "arangod",
      frames: [
        %{index: 0, function: "main", file: "main.cpp", line: 42},
        ...
      ]
    },
    ...
  ]
}
```


### ToastTest.SuiteRun

Per-suite execution context, passed through the Runner:

```elixir
%ToastTest.SuiteRun{
  suite_module: MySuite.Suite,
  deployment: %Toast.Deployment{...},  # nil until deploy succeeds
  suite_deadline: 1709123456789,       # monotonic_time(:millisecond)
  timeout_factor: 3.0
}
```


### ETS Tables

Toast uses ETS for cross-process shared state:

| Table | Type | Access | Purpose |
|-------|------|--------|---------|
| `:toast_suite_abort` | `:set` | `:public` | Abort signaling. Single `{:aborted, reason}` entry. |
| `:toast_deployment_registry` | `:set` | `:public` | Maps suite module to active deployment and extra context. |


## Module Dependency Map

Dependencies flow top-to-bottom. Each arrow means "calls/uses".

```
Mix.Tasks.Toast
Mix.Tasks.Toast.Gen.Suite
  |   uses: Mix.Tasks.Toast.Helpers (pure arg parsing)
  |
  v
ToastTest.Runner
  |   uses: ToastTest.ExUnitCompat (ExUnit internal shim)
  |         ToastTest.TestLifecycle (shared test lifecycle primitives)
  |         ToastTest.SuiteRun (data struct)
  |         ToastTest.Abort (ETS-backed abort state)
  |         ToastTest.DeploymentRegistry (ETS)
  |         ToastTest.ProcessHistory (GenServer)
  |         ToastTest.StateCleanup
  |         ToastTest.CrashMonitor (abort on crash)
  |         ToastTest.ResultCollector (ExUnit formatter)
  |         ToastTest.ArtifactCollector (filesystem artifact discovery)
  |         ToastTest.Attribution (issue production)
  |         ToastTest.SuiteResult (build + write results)
  |         ToastTest.PostExecSummary (CLI output)
  |
  v
Toast.Deployment (public API facade)
  |   uses: Toast.Config (configuration)
  |         Toast.Client (REST client)
  |         Toast.Deployment.Controller
  |         Toast.Deployment.ServerInstance
  |
  v
Toast.Deployment.Controller (GenServer + behaviour)
  |   uses: Toast.Deployment.Controller.State (defstruct)
  |         Toast.Deployment.Controller.Helpers (server lookup, update, health, relaunch)
  |         Toast.Deployment.ServerLifecycle (pure functions)
  |         Toast.Deployment.ServerInstance (data struct)
  |         Toast.Process.Supervisor (start children)
  |         Toast.Process.CrashInfo (data struct)
  |         Toast.Process.CrashEvent (data struct)
  |
  +---> Toast.Deployment.Controller.SingleServer (behaviour impl)
  |       uses: Toast.Deployment.Controller.Helpers
  |             Toast.Deployment.Factory
  |             Toast.Deployment.Health
  |             Toast.Deployment.ServerLifecycle
  |             Toast.Process.ServerProcess
  |             Toast.PortAllocator
  |
  +---> Toast.Deployment.Controller.Cluster (behaviour impl)
          uses: Toast.Deployment.Controller.Helpers
                Toast.Deployment.Factory
                Toast.Deployment.Health
                Toast.Deployment.ServerLifecycle
                Toast.Diagnostics.AgencyDump
                Toast.Process.ServerProcess
                Toast.PortAllocator

Toast.Deployment.Controller.Helpers
  |   uses: Toast.Deployment.ServerInstance (struct access)
  |         Toast.Deployment.ServerLifecycle (stop health monitor)
  |         Toast.Process.ServerProcess (stop, signal)
  |         Toast.Process.Supervisor (start health monitor)

Toast.Deployment.Factory
  |   uses: Toast.Deployment.CommandBuilder (arangod CLI args)
  |         Toast.Utils.Filesystem (dir creation, binary discovery)
  |         Toast.Diagnostics.Sanitizer (build env vars)
  |         Toast.Config

Toast.Process.ServerProcess (GenServer, wraps erlexec)
  |   no Toast dependencies -- communicates via messages

Toast.Process.CrashInfo (data struct, no dependencies)
Toast.Process.CrashEvent (data struct, no dependencies)

Toast.Process.HealthMonitor (GenServer, HTTP polling)
  |   uses: Req (HTTP client, direct -- not via Toast.Client)

Toast.Utils (utility functions, no Toast dependencies)

Toast.Diagnostics.Coredump
  |   uses: Toast.Diagnostics.Coredump.Debugger (behaviour)
  |         Toast.Diagnostics.Coredump.GDB (behaviour impl)
  |         Toast.Diagnostics.Coredump.LLDB (behaviour impl)
  |         Toast.Diagnostics.Coredump.Report (data struct)

Toast.Diagnostics.Summary
  |   uses: ToastTest.SuiteResult (struct access)

ToastTest.Abort (ETS-backed abort state, no Toast deps)

ToastTest.TestLifecycle (shared test lifecycle primitives, no Toast deps)

ToastTest.CLIFormatter (GenServer, no Toast deps)
ToastTest.ResultCollector (ExUnit formatter, stores test data)

ToastTest.ArtifactCollector (pure filesystem discovery)
  |   uses: Toast.Diagnostics.Coredump (discover core dumps)
  |         Toast.Deployment.ServerInstance (struct access)

ToastTest.Attribution (orchestrates issue production)
  |   uses: ToastTest.Attribution.TimeWindows (time window calculations)
  |         ToastTest.Enrichment.Coredump (coredump analysis)
  |         ToastTest.Enrichment.Logs (log parsing)
  |         ToastTest.Enrichment.Sanitizer (sanitizer file reading)
  |         Toast.Process.CrashEvent (struct access)

ToastTest.Attribution.TimeWindows (pure, no dependencies)

ToastTest.Enrichment.Logs (pure file I/O, no Toast deps)
ToastTest.Enrichment.Sanitizer (pure file I/O, no Toast deps)
ToastTest.Enrichment.Coredump
  |   uses: Toast.Diagnostics.Coredump (debugger analysis)

ToastTest.SuiteResult (central data struct)
  |   uses: ToastTest.SuiteResult.JSON (JSON serialization)
  |         ToastTest.SuiteResult.JUnitXML (JUnit XML serialization)
  |         ToastTest.ResultCollector (test_data type)

ToastTest.PostExecSummary (CLI output formatting)
  |   uses: ToastTest.SuiteResult (struct access)
  |         Toast.Utils (compact/1)

Toast.ResultPackaging (CI artifact packaging, no other Toast deps)

ToastTest.Case (ExUnit.CaseTemplate)
  |   uses: Toast.Client (create client for test context)
  |         ToastTest.DeploymentRegistry

ToastTest.Suite (macro module)
  |   uses: ToastTest.Case (via __using__ expansion)

ToastTest.Interactive
  |   uses: ToastTest.ExUnitCompat (ExUnit internal shim)
  |         ToastTest.TestLifecycle (shared test lifecycle primitives)
  |         ToastTest.DeploymentRegistry
```

### Dependency Principles

- **Toast.Deployment** is the only module that touches the Controller
  GenServer. All other code goes through the `Toast.Deployment` public API.

- **Mode callback modules** (SingleServer, Cluster) share a common set of
  helpers via `Controller.Helpers` and depend on the same subsystem modules
  (Factory, Health, ServerProcess, ServerLifecycle) but never depend on
  each other.

- **Attribution replaces the old Matcher/CrashMatcher/SanitizerMatcher
  hierarchy.** `Attribution` orchestrates issue production using
  `TimeWindows` (pure leaf) and the `Enrichment.*` modules (pure file I/O).
  This is a clean DAG with no circular dependencies.

- **Enrichment modules** (`Enrichment.Logs`, `Enrichment.Sanitizer`,
  `Enrichment.Coredump`) are pure file I/O with minimal Toast dependencies.
  They replaced the old `CrashLogParser` and `ServerLog` modules.

- **ToastTest.Runner** is the most connected module because it orchestrates
  the full test lifecycle: deployment, test execution, artifact collection,
  attribution, result building, and summary output. `ToastTest.Case` is now
  a thin ExUnit.CaseTemplate that only provides test context via the
  DeploymentRegistry.

- **Process modules** (ServerProcess, HealthMonitor) have zero Toast
  dependencies. They communicate purely via messages (`:server_crashed`,
  `:server_unhealthy`). This makes them independently testable and reusable.

- **External dependencies** are wrapped at boundaries:
  - `erlexec` is wrapped by `ServerProcess`
  - `Req` is wrapped by `Toast.Client` (except HealthMonitor which uses it
    directly for simplicity -- it only needs a single GET)
  - ExUnit internals are wrapped by `ExUnitCompat`
