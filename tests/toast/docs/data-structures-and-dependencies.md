# Data Structures and Dependencies

Key data structures and module dependency map for the TOAST test framework.
See [architecture.md](./architecture.md) for the system overview.


## Key Data Structures

### Toast.Deployment.t()

The public handle returned by `Toast.Deployment.start/3`. Passed to test
context and all deployment operations. Contains enough to address the
Controller GenServer but not the full internal state.

```elixir
%Toast.Deployment{
  id: "single-00",                    # unique deployment identifier
  controller: #PID<0.123.0>,          # Controller GenServer pid
  api_version: 1,                     # API version prefix (nil, integer, or string)
  servers: %{                         # Map of server_id => ServerInfo
    "single-00" => %ServerInfo{
      id: "single-00",
      role: :single,
      port: 8529,
      endpoint: "http://127.0.0.1:8529",
      arango_id: nil
    }
  },
  jwt_provider: nil                   # Toast.JWT.Provider (nil when auth disabled)
}
```

Why this exists as a separate struct from Controller.State: the Controller.State
is internal to the GenServer and contains mutable runtime state (server pids,
health monitors, crash tracking). The Deployment struct is a stable reference
that test code can hold without coupling to GenServer internals.

The `servers` map contains `ServerInfo` structs -- lightweight, immutable
snapshots taken at deploy time. These are distinct from `ServerInstance` which
carries mutable runtime state.


### Toast.Deployment.Controller.State

Internal state of the Controller GenServer. Not exposed outside the GenServer
process. See [architecture.md](./architecture.md#controllerstate) for the full
breakdown.

```elixir
%Controller.State{
  config: %Toast.Deployment.Config{},  # Deployment configuration
  id: "single-00",                     # Deployment ID
  status: :ready,                      # :stopped | :starting | :ready | :degraded | :stopping | :failed
  servers: %{                          # Map of server_id => ServerInstance
    "single-00" => %ServerInstance{...}
  },
  expected_crashes: %{},               # Map of server_id => CrashExpectation
  error: nil,                          # Error details if status is :failed
  agency_dump: nil,                    # Captured agency state (cluster only)
  event_listener: ToastTest.DeploymentListener,  # EventListener behaviour impl
  jwt_provider: nil                    # Toast.JWT.Provider (nil when auth disabled)
}
```


### Toast.Deployment.ServerInstance.t()

Runtime state of a single server process within a deployment:

```elixir
%Toast.Deployment.ServerInstance{
  id: "single-00",               # server identifier
  role: :single,                 # :single | :agent | :dbserver | :coordinator
  port: 8529,                    # TCP port for HTTP API
  endpoint: "http://127.0.0.1:8529",
  pid: 12345,                    # OS process ID (from erlexec)
  log_file: "/tmp/toast/.../arangod.log",
  server_dir: "/tmp/toast/.../",
  server_pid: #PID<0.456.0>,    # ServerProcess GenServer pid
  health_monitor: #PID<0.789.0>,# HealthMonitor GenServer pid
  operational_state: :running,   # :running | :paused | :stopped | :killed | :crashed
  intentional: false,            # true when state change was requested by test code
  expecting_exit: false,         # true during stop/kill (before process exit arrives)
  arango_id: nil,                # ArangoDB-assigned internal ID (cluster only)
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


### Toast.Deployment.Config.t()

Deployment infrastructure configuration. Immutable after `Config.new/1`. All
timeouts are pre-multiplied by `timeout_factor` (via `Toast.Env`).

```elixir
%Toast.Deployment.Config{
  # Paths
  build_dir: "/path/to/build",       # where arangod binary lives

  # Server args
  server_args: %{},                   # map of option => value for all/single servers
  show_server_logs: false,            # print arangod stderr

  # Sanitizer
  sanitizer_override: :alubsan,       # forced sanitizer type, or nil
  active_sanitizers: #MapSet<["ASAN_OPTIONS", ...]>,  # active env var names

  # Cluster
  cluster: nil,                       # nil for single server, ClusterOpts for cluster

  # Timeouts (milliseconds, pre-multiplied by timeout_factor)
  startup_timeout: 60_000,
  shutdown_timeout: 60_000,
  timeout_factor: 1,                  # multiplier (3 for sanitizer, 10 for rr)

  # Protocol
  api_version: nil,                   # API version prefix for URLs
  protocol: :http1,                   # :http1 | :http2
  ssl: false,                         # enable SSL/TLS

  # Authentication
  authentication: false,              # enable JWT authentication
  jwt_algorithm: :hmac,               # :hmac | :ecdsa

  # Recording
  rr: nil,                            # MapSet of roles to record with rr
  rr_path: nil,                       # path to rr executable

  # Resources
  memory_budget: nil                  # memory budget in bytes (auto-detected)
}
```


### ToastTest.Config.t()

Test execution configuration. Separate from Deployment.Config because it
covers test runner concerns, not deployment infrastructure.

```elixir
%ToastTest.Config{
  base_dir: "/tmp/toast/...",         # per-run server data
  result_dir: "toast-results",        # where results are written
  deployment_mode: :single_server,    # :single_server | :cluster | :manual
  timeout_factor: 1,                  # multiplier
  global_timeout: 3_600_000,          # entire test run
  test_timeout: 300_000,              # single test
  keep_data: false,                   # preserve server data after run
  ci: false,                          # enable CI packaging
  debugger: :auto,                    # :gdb | :lldb | :auto | :none
  attach_debugger: false,             # pause for debugger attachment
  coredump_timeout: 180_000,          # coredump analysis budget
  coredump_dir: nil,                  # explicit coredump search directory
  dump_agency_on_error: true          # capture agency state on failure
}
```


### Toast.Client.t()

REST client wrapping `Req`. Supports database scoping, API versioning, and
multiple authentication modes:

```elixir
%Toast.Client{
  base_url: "http://127.0.0.1:8529",
  database: "mydb",                  # nil for system-level requests
  api_version: 1,                    # integer, string, or nil
  auth: {:basic, "root", ""},        # {:basic, u, p} | {:jwt, token} | {:jwt_provider, provider} | nil
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
| `ToastTest.EventStore` | `:ordered_set` | `:public` | Chronological lifecycle events keyed by `{timestamp, sequence}`. |


## Module Dependency Map

Dependencies flow top-to-bottom. Each arrow means "calls/uses".

```
Mix.Tasks.Toast
Mix.Tasks.Toast.Gen.Suite
  |   uses: Mix.Tasks.Toast.Helpers (pure arg parsing)
  |         Toast.Env (configuration resolution)
  |
  v
ToastTest.Runner
  |   uses: ToastTest.ExUnitCompat (ExUnit internal shim)
  |         ToastTest.TestLifecycle (shared test lifecycle primitives)
  |         ToastTest.Runner.TestExecution (core test execution loop)
  |         ToastTest.Runner.TestProcess (test process spawn/management)
  |         ToastTest.Runner.TestFilter (test filtering)
  |         ToastTest.Runner.BetweenTests (health checks between tests)
  |         ToastTest.Runner.PostExecution (post-suite diagnostics)
  |         ToastTest.Runner.ResultBuilder (SuiteResult assembly)
  |         ToastTest.Runner.FailureFormatter (failure display)
  |         ToastTest.Runner.Timeout (deadline management)
  |         ToastTest.SuiteRun (data struct)
  |         ToastTest.Abort (ETS-backed abort state)
  |         ToastTest.DeploymentRegistry (ETS)
  |         ToastTest.DeploymentListener (EventListener impl)
  |         ToastTest.CrashMonitor (abort on crash)
  |         ToastTest.ResultCollector (ExUnit formatter)
  |         ToastTest.ArtifactCollector (filesystem artifact discovery)
  |         ToastTest.Attribution (issue production)
  |         ToastTest.SuiteResult (build + write results)
  |         ToastTest.Formatting.PostExecSummary (CLI output)
  |         ToastTest.Formatting.RunSummary (overall run summary)
  |         ToastTest.Formatting.RrSummary (rr recording summary)
  |         ToastTest.WithDeployment (scoped per-test deployments)
  |         ToastTest.DeployConfig (deployment config builder)
  |         Toast.Deployment.CrashBarrier (between-tests crash detection)
  |         Toast.Deployment.HealthBarrier (between-tests health check)
  |         Toast.Deployment.Netstat (between-tests port exhaustion check)
  |
  v
Toast.Deployment (public API facade)
  |   uses: Toast.Deployment.Config (configuration)
  |         Toast.Client (REST client)
  |         Toast.Deployment.Controller
  |         Toast.Deployment.ServerInstance
  |         Toast.Deployment.Factory
  |         Toast.JWT.KeyGen (JWT material generation)
  |         Toast.JWT.Provider (JWT signer handle)
  |
  v
Toast.Deployment.Controller (GenServer)
  |   uses: Toast.Deployment.Controller.State (nested defstruct)
  |         Toast.Deployment.DeployPipeline (multi-phase deploy)
  |         Toast.Deployment.ShutdownPipeline (ordered shutdown/rollback/abort)
  |         Toast.Deployment.ServerLifecycle (pure functions)
  |         Toast.Deployment.ServerInstance (data struct)
  |         Toast.Deployment.CrashExpectation (data struct)
  |         Toast.Deployment.Events (event emission)
  |         Toast.Deployment.EventListener (behaviour)
  |         Toast.Process.CrashInfo (data struct)
  |         Toast.Diagnostics.AgencyDump (cluster agency dump)

Toast.Deployment.DeployPipeline
  |   uses: Toast.Deployment.Events (event emission)
  |         Toast.Deployment.Health (HTTP health checks)
  |         Toast.Deployment.ServerLifecycle (server ops)
  |         Toast.Process.ServerProcess (launch OS processes)
  |         Toast.Process.Supervisor (start children)

Toast.Deployment.ShutdownPipeline
  |   uses: Toast.Deployment.ServerLifecycle (stop servers)
  |         Toast.Process.ServerProcess (stop, signal)
  |         Toast.Process.Supervisor (stop health monitors)

Toast.Deployment.Factory
  |   uses: Toast.Deployment.CommandBuilder (arangod CLI args)
  |         Toast.Deployment.ClusterOpts (cluster topology)
  |         Toast.Utils.Filesystem (dir creation, binary discovery)
  |         Toast.Diagnostics.Sanitizer (build env vars)
  |         Toast.Deployment.Config

Toast.Process.ServerProcess (GenServer, wraps erlexec)
  |   no Toast dependencies -- communicates via messages

Toast.Process.CrashInfo (data struct, no dependencies)

Toast.Process.HealthMonitor (GenServer, HTTP polling)
  |   uses: Req (HTTP client, direct -- not via Toast.Client)

Toast.JWT.KeyGen (JWT key generation)
  |   uses: :crypto, :public_key (Erlang stdlib)

Toast.JWT.Provider (data struct, holds signer)
  |   uses: Toast.JWT (which uses Joken for signing)

Toast.Utils (utility functions, no Toast dependencies)

Toast.Diagnostics.Coredump
  |   uses: Toast.Diagnostics.Coredump.Debugger (behaviour)
  |         Toast.Diagnostics.Coredump.GDB (behaviour impl)
  |         Toast.Diagnostics.Coredump.LLDB (behaviour impl)
  |         Toast.Diagnostics.Coredump.Report (data struct)

ToastTest.Abort (ETS-backed abort state, no Toast deps)

ToastTest.TestLifecycle (shared test lifecycle primitives, no Toast deps)

ToastTest.Formatting.CLI (GenServer, no Toast deps)
ToastTest.ResultCollector (ExUnit formatter, stores test data)

ToastTest.EventStore (ETS-backed event store)
  |   uses: ToastTest.EventStore.Projections (derived views)

ToastTest.DeploymentListener (EventListener implementation)
  |   uses: ToastTest.EventStore (event recording)
  |         ToastTest.CrashMonitor (crash notification)

ToastTest.ArtifactCollector (pure filesystem discovery)
  |   uses: Toast.Diagnostics.Coredump (discover core dumps)
  |         Toast.Deployment.ServerInstance (struct access)

ToastTest.Attribution (orchestrates issue production)
  |   uses: ToastTest.Attribution.TimeWindows (time window calculations)
  |         ToastTest.Attribution.ServerLogs (server log data)
  |         ToastTest.Enrichment.Coredump (coredump analysis)
  |         ToastTest.Enrichment.Logs (log parsing)
  |         ToastTest.Enrichment.Sanitizer (sanitizer file reading)
  |         ToastTest.CrashEvent (struct access)

ToastTest.Attribution.TimeWindows (pure, no dependencies)

ToastTest.Enrichment.Logs (pure file I/O, no Toast deps)
ToastTest.Enrichment.Sanitizer (pure file I/O, no Toast deps)
ToastTest.Enrichment.Coredump
  |   uses: Toast.Diagnostics.Coredump (debugger analysis)

ToastTest.SuiteResult (central data struct)
  |   uses: ToastTest.SuiteResult.JSON (JSON serialization)
  |         ToastTest.SuiteResult.JUnitXML (JUnit XML serialization)
  |         ToastTest.ResultCollector (test_data type)

ToastTest.Formatting.PostExecSummary (CLI output formatting)
  |   uses: ToastTest.SuiteResult (struct access)
  |         ToastTest.Formatting (shared utilities)

ToastTest.Formatting.RunSummary (overall run summary)
  |   uses: ToastTest.SuiteResult (struct access)

ToastTest.DiagnosticsSummary (aggregate diagnostics)
  |   uses: ToastTest.SuiteResult (struct access)

ToastTest.ResultPackaging (CI artifact packaging)
  |   uses: Toast.Utils.Compression (compression tools)

ToastTest.Case (ExUnit.CaseTemplate)
  |   uses: Toast.Client (create client for test context)
  |         ToastTest.DeploymentRegistry
  |         ToastTest.Expect (imported into test modules)

ToastTest.Suite (macro module)
  |   uses: ToastTest.Case (via __using__ expansion)

ToastTest.JavascriptSuite (macro module)
  |   uses: ToastTest.Suite (via __using__ expansion)

ToastTest.JS.TestRunner
  |   uses: ToastTest.JS.ResultMapper (map results to ExUnit structs)
  |         ToastTest.JS.Executor (behaviour)
  |         ToastTest.ExUnitCompat (ExUnit event emission)
  |         ToastTest.EventStore (lifecycle events)

ToastTest.JS.ArangoshExecutor (Executor implementation)
  |   uses: Jason (JSON parsing)

ToastTest.JS.ModuleBuilder (pure, no runtime dependencies)

ToastTest.JS.ResultMapper (pure, no dependencies)

ToastTest.Interactive
  |   uses: ToastTest.ExUnitCompat (ExUnit internal shim)
  |         ToastTest.TestLifecycle (shared test lifecycle primitives)
  |         ToastTest.DeploymentRegistry

ToastTest.WithDeployment (scoped deployment helper)
  |   uses: ToastTest.DeployConfig (config builder)
  |         Toast.Deployment (start/stop)
  |         ToastTest.DeploymentEventListener (event listener)

ToastTest.DeployConfig (pure config builder, minimal deps)
  |   uses: Toast.Deployment.Config

ToastTest.DeploymentEventListener (EventListener for manual deployments)
  |   uses: ToastTest.EventStore (event recording)

Toast.Deployment.CrashBarrier (between-tests crash check)
  |   uses: Toast.Process.ProcStatus (/proc/<pid>/status reader)
  |         Toast.Utils.Polling (poll-until utility)

Toast.Deployment.HealthBarrier (between-tests health check)
  |   uses: Toast.Process.HealthMonitor (health probe state)
  |         Toast.Deployment.ServerLifecycle (probe health monitor)
  |         Toast.Utils.Polling (poll-until utility)

Toast.Deployment.Netstat (between-tests port exhaustion check)
  |   uses: ss or netstat (external, detected at startup)

Toast.Process.ProcStatus (reads /proc/<pid>/status, no Toast deps)

Toast.Utils.Polling (generic polling utility, no Toast deps)

Mix.Tasks.Toast.Analyze
  |   uses: Mix.Tasks.Toast.Analyze.Data (data loading)
  |         Mix.Tasks.Toast.Analyze.Issues (issue listing)
  |         Mix.Tasks.Toast.Analyze.Detail (detailed diagnostics)
  |         Mix.Tasks.Toast.Analyze.Info (file overview)
  |         Mix.Tasks.Toast.Analyze.Perf (performance analysis)
  |         Mix.Tasks.Toast.Analyze.Weights (weight suggestions)
  |         ToastTest.LogAnalysis (log data transformation)
```

### Dependency Principles

- **Toast.Deployment** is the only module that touches the Controller
  GenServer. All other code goes through the `Toast.Deployment` public API.

- **DeployPipeline and ShutdownPipeline** handle the complex multi-phase
  startup and shutdown sequences as pure pipeline functions that operate on
  Controller.State. The Controller delegates to them and focuses on message
  routing and state management.

- **Attribution replaces the old Matcher/CrashMatcher/SanitizerMatcher
  hierarchy.** `Attribution` orchestrates issue production using
  `TimeWindows` (pure leaf) and the `Enrichment.*` modules (pure file I/O).
  This is a clean DAG with no circular dependencies.

- **Enrichment modules** (`Enrichment.Logs`, `Enrichment.Sanitizer`,
  `Enrichment.Coredump`) are pure file I/O with minimal Toast dependencies.

- **ToastTest.Runner** is the most connected module because it orchestrates
  the full test lifecycle. Its complexity is managed by delegation to
  focused submodules: `TestExecution`, `TestProcess`, `BetweenTests`,
  `PostExecution`, `ResultBuilder`, `TestFilter`, and `Timeout`.

- **Process modules** (ServerProcess, HealthMonitor) have zero Toast
  dependencies. They communicate purely via messages (`:server_crashed`,
  `:server_unhealthy`). This makes them independently testable and reusable.

- **Event system** provides loose coupling between deployment lifecycle and
  test execution. The `EventListener` behaviour allows different consumers
  (test runner vs interactive mode) without the deployment layer knowing
  about test execution concerns.

- **Configuration is split into three layers**: `Toast.Env` resolves from
  all sources, `Toast.Deployment.Config` holds deployment concerns,
  `ToastTest.Config` holds test execution concerns. This reflects the
  architectural boundary between infrastructure and test runner.

- **External dependencies** are wrapped at boundaries:
  - `erlexec` is wrapped by `ServerProcess`
  - `Req` is wrapped by `Toast.Client` (except HealthMonitor which uses it
    directly for simplicity -- it only needs a single GET)
  - ExUnit internals are wrapped by `ExUnitCompat`
  - `Joken` is wrapped by `Toast.JWT.*`
