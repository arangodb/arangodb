# Toast Implementation Plan

## 1. Context and Goals

Toast is an Elixir framework that manages ArangoDB server deployments for integration testing. It currently handles the full lifecycle: starting arangod processes, waiting for health, running ExUnit tests against a live server, collecting diagnostics (crash logs, sanitizer output), and exporting structured results.

The framework needs to evolve in several ways:

1. **Infrastructure as library**: The deployment management layer should be usable outside of ExUnit — in IEx sessions, scripts, and future consumers — not coupled to the test framework.
2. **Suite system**: Tests need a suite abstraction where suites declare and manage their own deployments, replacing the current single-deployment-per-run model.
3. **Resilience testing**: Tests that deliberately manipulate servers (stop, pause, crash, restart) need first-class support, with health monitoring aware of intentional actions.
4. **Project simplification**: The current umbrella structure should flatten to a single Mix project with clear separation between framework code, unit tests, and integration test suites.
5. **Additional tooling**: Coredump analysis (GDB), result packaging for CircleCI, analysis CLI, and an extensible REST client.

The existing process management (erlexec-based), health monitoring, diagnostics (crash parsing, sanitizer collection), and result export layers are solid and should be preserved. The changes are structural and additive, not a rewrite.

Timeline is incremental over months with no hard deadline. The plan is organized into phases that can be delivered independently.

---

## 2. Project Structure

### Current State

```
tests/elixir/toast/
  apps/
    toast/           # Core framework (umbrella app)
      lib/toast/
      test/
    smoke_test/      # Example test suite (umbrella app)
      lib/
      test/
  mix.exs            # Umbrella root
```

### Target State

```
tests/elixir/toast/
  lib/
    toast/
      deployment/        # Deployment orchestration (existing, refactored)
      process/           # OS process management (existing, preserved)
      diagnostics/       # Crash/sanitizer/log analysis (existing, extended)
      client/            # REST client (new, extensible)
      analysis/          # Result analysis tooling (new)
      config.ex          # Configuration (existing, preserved)
      port_allocator.ex  # Port allocation (existing, preserved)
      log_formatter.ex   # Logging (existing, preserved)
      application.ex     # OTP application (existing, refactored)
    toast_test/
      runner.ex          # Custom test runner (existing, refactored)
      case.ex            # Base test case template (existing, refactored)
      suite.ex           # Suite definition and discovery (new)
      crash_monitor.ex   # Crash monitor that calls Runner.abort! (new, extracted)
      cli_formatter.ex   # CLI output formatter (existing, preserved)
      result_formatter.ex # Result event collector (existing, preserved)
      result_exporter/   # JSON + JUnit XML export (existing, preserved)
  test/                  # Unit tests for the framework
    toast/
      deployment/
      process/
      diagnostics/
      client/
      ...
  suites/                # Integration/system test suites
    smoke/               # Basic smoke tests (migrated from apps/smoke_test)
    shell_server/        # Collection/document/AQL tests
    resilience/          # Server failure/recovery tests
    ...
  mix.exs
```

### Design Rationale

**Single project over umbrella**: The umbrella was originally needed to work around ExUnit's module execution ordering. The custom mix task and runner eliminate that need. A single project simplifies dependency management and makes it clear that `toast_test` depends on `toast` (the library), not the other way around.

**`lib/toast/` vs `lib/toast_test/`**: The key architectural boundary. `toast` is the deployment infrastructure library — it has no knowledge of ExUnit, test cases, or suites. `toast_test` is the test framework that builds on `toast` to provide ExUnit integration. This separation enables the "infrastructure as library" goal: IEx sessions and scripts use `toast` directly; test suites use `toast_test`.

**`suites/` directory**: Named "suites" rather than "integration" or "system" because it directly reflects the domain concept. Each subdirectory is a self-contained test suite with its own deployment requirements. The custom mix task handles discovery and loading of suite modules — ExUnit's default test file discovery is not used for suites.

**`test/` directory**: Standard ExUnit unit tests for the framework itself. These test the library modules in isolation (mocking erlexec, HTTP calls, etc.) and run with standard `mix test`. They are separate from integration suites that require a running ArangoDB.

**Module naming**: The current `Toast.TestCase` becomes `ToastTest.Case`, and the new suite behaviour lives at `ToastTest.Suite`. The `Toast.*` namespace is exclusively for the infrastructure library; `ToastTest.*` is for the test framework. This naming convention makes the boundary explicit.

---

## 3. Infrastructure Library (`lib/toast/`)

### 3.1 Deployment API

The deployment module provides a high-level API for managing ArangoDB deployments. It is the primary entry point for all consumers (test framework, IEx, scripts).

#### Deployment Struct

The deployment struct is a **handle** to a running deployment, not a snapshot of its state:

```elixir
%Toast.Deployment{
  id: String.t(),
  mode: :single_server | :cluster,
  config: Toast.Config.t(),
  controller: pid(),
  endpoint: String.t(),
  work_dir: Path.t()
}
```

The struct contains the controller PID (for live queries) plus immutable fields that are set at startup and never change: `endpoint` and `work_dir` (the deployment's working directory). These are on the struct to avoid unnecessary GenServer round-trips for values that are constant after deployment startup.

**Note on `endpoint`**: For a single-server deployment, `endpoint` is the server's URL. For a cluster deployment, there is no single canonical endpoint — `endpoint` is set to the URL of the first coordinator as a convenience for the common case (most client operations go through a coordinator). For direct access to specific servers (dbservers, agents, other coordinators), use `Toast.Deployment.client/2` (see Section 5).

The struct does NOT contain mutable state like `status` or `servers`. These are queried live from the controller GenServer via functions, preventing stale-data issues.

**Migration note**: The existing struct also has `crash_monitor` and `servers` fields. The `crash_monitor` field is removed in Phase 2 when the `:on_crash` callback replaces the dedicated crash monitor process (the `spawn_crash_monitor/0` and `stop_crash_monitor/1` functions are deleted). The `servers` field is replaced by the `servers/1` query function.

#### Core Operations

The deployment module exposes these operations:

- `start(mode, opts)` — Start a deployment (single server or cluster). Returns `{:ok, deployment}` or `{:error, reason}`.
- `stop(deployment)` — Graceful shutdown of all servers. Returns `:ok`.
- `stop_and_collect(deployment)` — Stop, collect diagnostics (including coredump analysis as a post-shutdown step with its own timeout), then return `diagnostics` (a map, or `nil` if collection failed).
- `status(deployment)` — Query current deployment status. Returns `:starting | :ready | :degraded | :stopping | :stopped | :failed`.
- `server(deployment, server_id)` — Get current state of a specific server (live query to controller).
- `servers(deployment)` — List all servers with current state.
- `servers(deployment, role: role)` — List servers filtered by role.
- `endpoint(deployment)` — Get the primary endpoint (coordinator for cluster, server for single).
- `crash_info(deployment)` — Get crash details if status is `:failed`.

The `:degraded` status indicates that some servers are intentionally down (paused, stopped by test) but the deployment is still operational. This is the key addition for resilience testing support.

**Dead handle**: After `stop/1` or `stop_and_collect/1`, the controller process terminates and the deployment struct becomes a "dead handle". Calling query functions (`status/1`, `servers/1`, etc.) on a stopped deployment returns `{:error, :stopped}` (the GenServer call catches the `:noproc` exit). Tests should not retain deployment handles after teardown.

#### Server Control Operations (New)

For resilience testing and interactive use, the deployment exposes per-server control. All operations return `:ok | {:error, reason}` — they mutate the controller's internal state, not the deployment struct.

- `stop_server(deployment, server_id)` — Graceful stop of one server. Marks it as "intentionally stopped" so monitoring does not trigger a crash abort.
- `kill_server(deployment, server_id)` — Send SIGKILL. Marks as "intentionally killed".
- `pause_server(deployment, server_id)` — Send SIGSTOP. Server process exists but is frozen. Marks as "paused".
- `resume_server(deployment, server_id)` — Send SIGCONT to a paused server. Resumes monitoring.
- `restart_server(deployment, server_id, opts)` — Stop then start a server. `opts` accepts additional/override CLI arguments via `args: [...]` which are merged with the original launch spec. Port, data directory, and binary are immutable across restarts.
- `start_server(deployment, server_id, opts)` — Start a previously stopped server.
- `expect_crash(deployment, server_id, opts \\ [])` — Mark a server as "expected to crash". The next crash on this server is treated as intentional (monitoring suppressed, no crash callback). Returns `:ok`. Options: `timeout: milliseconds` (default: 30_000) — how long the expectation stays active before auto-clearing. At most one pending expectation per server (concurrent ops serialized by GenServer mailbox), so server_id is sufficient for identification — no ref needed.
- `verify_crash(deployment, server_id, opts \\ [])` — Verify that an expected crash actually happened. Returns `{:ok, crash_info}` if the server crashed as expected, `{:error, :not_crashed}` if the server is still running, or `{:error, :timeout}` after a configurable wait (default: uses the expectation's timeout).

The `expect_crash` mechanism handles the case where a test sets a failure point that will cause the server to crash at a specific point (e.g., during a subsequent request). The test cannot use `kill_server` because the crash is triggered indirectly by application logic, not by a signal. The flow is:

1. Test calls `expect_crash(deployment, "dbserver-1")` → health monitoring suspended
2. Test triggers the action that causes the crash (e.g., a request that hits the failure point)
3. Server crashes → controller records the crash as intentional (because `expect_crash` was set)
4. Test calls `verify_crash(deployment, "dbserver-1")` to confirm the crash happened
5. Test restarts the server: `restart_server(deployment, "dbserver-1")`

If the expected crash does not happen within the timeout (default: 30 seconds, configurable per call), the expectation is automatically cleared and monitoring resumes. **Edge case**: If the crash fires slightly after the timeout clears the expectation, it will be treated as an unexpected crash. The runner's `:ready` health check between tests catches this — the suite aborts with a clear error rather than silently continuing with a crashed server.

All control operations are synchronous and return when the action is confirmed. Health monitoring is automatically adjusted:
- When a server is intentionally stopped/killed/paused, the health monitor is suspended for that server (pending timer cancelled).
- When `expect_crash` is called, the health monitor is suspended for that server preemptively.
- When a server is restarted/resumed, health monitoring resumes and waits for the server to become healthy.
- If monitoring detects a failure on a server that was NOT intentionally manipulated, the crash notification callback is invoked (see Section 3.2).

This design means tests never need to manually toggle monitoring. The framework knows whether a server's state was caused by a test action or an unexpected crash.

**Constraint**: Control operations on the same server must not be concurrent. The controller processes them serially via GenServer calls. Concurrent calls from different processes are serialized by the GenServer mailbox.

**Server targeting**: Control operations accept `server_id` which identifies a specific server. For convenience, role-based targeting is also supported:

- `stop_server(deployment, role: :dbserver)` — Stop all servers with the given role
- `pause_server(deployment, role: :coordinator, index: 0)` — Target the first coordinator

This avoids tests needing to know internal server IDs when they just need "a dbserver" or "the second coordinator".

#### Crash Notification (Library-Test Boundary)

The deployment accepts an optional `:on_crash` callback when started:

```elixir
Toast.Deployment.start(:cluster,
  build_dir: "/path",
  on_crash: fn deployment, crash_info -> ... end
)
```

The `toast` library calls this callback when an unexpected crash is detected. It does NOT know what the callback does — it could abort a test suite, log an event, or send an alert. The `toast_test` framework provides a callback that calls `ToastTest.Runner.abort!/1`. This breaks the coupling between the library and the test framework.

When no callback is provided (e.g., IEx use), crashes are logged but no external action is taken.

### 3.2 Controller Architecture

#### Server State Tracking

Each server has an operational state tracked by the controller:

```elixir
%{
  instance: ServerInstance.t(),
  operational_state: :running | :paused | :stopped | :killed | :crashed,
  intentional: boolean()
}
```

The `intentional` flag solves the resilience monitoring question:

- When `stop_server/2` is called, state transitions to `{:stopped, intentional: true}`. Health monitor is suspended.
- When an unexpected crash is detected (erlexec `:DOWN` message arrives without a prior control operation), state transitions to `{:crashed, intentional: false}`. The `:on_crash` callback fires.
- When `restart_server/2` or `resume_server/2` is called, `intentional` resets to `false` and monitoring resumes.

**Signal-type awareness for crash-during-intentional-stop**: If a control operation (e.g., `stop_server`) sends SIGTERM and the server crashes with a different signal (SIGSEGV during shutdown), the controller examines the exit signal. A SIGTERM exit is treated as intentional. A SIGSEGV/SIGABRT/SIGBUS exit during an intentional stop is treated as an **unexpected crash** — the intentional flag is cleared and the crash callback fires. This prevents masking real bugs in shutdown code.

**Race condition handling**: If a crash message arrives in the controller's mailbox before a `stop_server` call is processed, the crash is treated as unexpected (correct behavior — the server died independently). The subsequent `stop_server` call returns `{:error, :already_crashed}`.

#### SingleServerController Changes

The existing SingleServerController GenServer gains `handle_call` clauses for the new control operations. The state machine transitions expand:

```
:ready → stop_server → :stopped (intentional)
:ready → kill_server → :stopped (intentional)
:ready → pause_server → :paused (intentional)
:paused → resume_server → :ready
:stopped → start_server → :starting → :ready
:ready → (unexpected crash) → :crashed (not intentional)
:stopped(intentional) → (SIGSEGV during shutdown) → :crashed (not intentional, flag cleared)
```

#### ClusterController Changes

The ClusterController wraps the same operations but operates on individual servers within the cluster. It maintains a map of per-server states and updates the deployment-level status:

- All servers running → `:ready`
- Some servers intentionally down → `:degraded`
- Any server unexpectedly crashed → `:failed`

#### HealthMonitor Process Monitoring

The controller monitors its HealthMonitor processes using `Process.monitor/1`. If a HealthMonitor crashes due to a bug (not a server failure), the controller detects the `:DOWN` message and restarts the monitor. This prevents silent loss of health monitoring.

### 3.3 Health Monitor Updates

The HealthMonitor GenServer currently polls a server's HTTP endpoint and notifies the controller on consecutive failures. Changes:

- Accept a `:suspend` message to pause polling. This cancels any pending `Process.send_after` timer to prevent stale `:check` messages from firing after suspension.
- Accept a `:resume` message to restart polling.
- The controller sends `:suspend` when a control operation intentionally changes a server's state, and `:resume` when the server is brought back.

The HealthMonitor's `status` field gains a `:suspended` value alongside the existing `:healthy` and `:unhealthy`. The `healthy?/1` function is replaced by a `status/1` function returning `:healthy | :unhealthy | :suspended`. A suspended monitor is not queried for health — callers (the controller) know the server is intentionally down.

No changes to the HealthMonitor's core polling or failure detection logic.

### 3.4 Application Supervision Tree

The supervision tree refactors slightly for the new structure:

```
Toast.Application
├── Toast.PortAllocator (GenServer)
├── Toast.Process.Supervisor (DynamicSupervisor)
│   ├── Toast.Process.ServerProcess (per server, temporary)
│   └── Toast.Process.HealthMonitor (per server, temporary)
└── Toast.Deployment.Supervisor (DynamicSupervisor)
    ├── Toast.Deployment.SingleServerController (temporary)
    └── Toast.Deployment.ClusterController (temporary)
```

All child processes use `:temporary` restart strategy — crashed servers are diagnostic events, not something to auto-recover. Controllers monitor their HealthMonitor children for unexpected crashes and restart them.

### 3.5 Failure Point Management

For resilience testing, the deployment exposes failure injection via ArangoDB's debug API:

- `set_failure_point(deployment, server_id_or_role, name)` — Call `/_admin/debug/failat/{name}` on specific server(s)
- `clear_failure_point(deployment, server_id_or_role, name)` — Remove a specific failure point
- `clear_all_failure_points(deployment)` — Clear all failure points on all servers

The second argument accepts the same targeting as server control operations: a specific server ID, or role-based targeting like `role: :dbserver` (set on all dbservers) or `role: :coordinator, index: 0` (set on first coordinator only).

These are thin wrappers over the REST API, available only when ArangoDB is built with failure point support (debug builds). They are part of the deployment API (not the client) because they operate on the deployment infrastructure, not on user data.

### 3.6 Configuration

The existing `Toast.Config` module is preserved with an extended precedence chain: keyword opts > environment variables > local config file > defaults.

**Local config file**: A `.toast.local.exs` file (gitignored) in the project root allows developers to set machine-specific configuration (e.g., build directory, debugger path) without affecting the repository. The file is evaluated at startup if present and its values merge into the config defaults. This is particularly useful for developers with different build setups. **CI safety**: When `--ci` or `TOAST_CI=true` is set, `.toast.local.exs` is NOT evaluated. This prevents developer-specific configuration from affecting CI runs and eliminates the security concern of evaluating arbitrary Elixir code in CI environments.

```elixir
# .toast.local.exs — not checked into Git
%{
  build_dir: "/home/dev/arangodb/build-release",
  debugger: :lldb
}
```

**Important for the suite system**: Suite deployment configuration is passed as explicit keyword options to `Toast.Deployment.start/2`, NOT by mutating environment variables. The current `apply_toast_env/1` in the mix task (which calls `System.put_env`) is refactored: environment variables are read once at task startup and passed as keyword opts to each suite's deployment. This prevents env var pollution between sequential suite executions.

---

## 4. Test Framework (`lib/toast_test/`)

### 4.1 Suite System Design

#### The Problem

The current Toast has a single deployment per test run. All tests share it. The JS framework has "suites" — high-level groupings where each suite manages its own deployment. Some suites use standard deployments (single or cluster), while others (resilience, replication) have custom deployment logic.

#### Proposed Approach: Suite Modules with CaseTemplates

Each suite is defined by a module that declares:
1. What deployment it needs (deployment mode, CLI arguments, configuration overrides)
2. Optional custom deployment setup/teardown logic
3. A CaseTemplate that test modules `use`

```elixir
defmodule ToastTest.Suite do
  @callback deployment_config() :: keyword()
  @callback setup_deployment(Toast.Deployment.t()) :: {:ok, map()} | {:error, term()}
  @callback teardown_deployment(Toast.Deployment.t()) :: :ok
  @optional_callbacks [setup_deployment: 1, teardown_deployment: 1]
end
```

**Callback timing**: `setup_deployment/1` runs **once per suite**, after the deployment starts and passes health checks, before any tests run. It is NOT part of the per-test setup chain. Its return value (`{:ok, extra_context}`) is stored by the runner and merged into every test's context alongside the base context from `ToastTest.Case`. Keys in the extra context override the base context (explicit override semantics — a suite can provide a custom `:client` that replaces the default). Use this for one-time suite-level work: validating cluster topology, seeding shared data, creating test databases, etc. Per-test setup uses standard ExUnit `setup` blocks in test modules.

**Error handling**: If `setup_deployment/1` returns `{:error, reason}`, all tests in the suite are marked as `:errored` with the failure reason. The deployment is shut down via `stop_and_collect/1` and the runner proceeds to the next suite. This is analogous to the deployment-failure-to-start handling in Section 4.2.

**Optional callback dispatch**: The runner checks `function_exported?(suite_module, :setup_deployment, 1)` before calling. Suites without this callback simply use the base context.

A suite module implements this behaviour and uses `ToastTest.Suite` to get a CaseTemplate:

```elixir
defmodule ShellServer.Suite do
  use ToastTest.Suite,
    timeout: 600_000,
    server_args: ["--javascript.enabled", "true"]
end
```

**Deployment mode**: Most suites should NOT specify a deployment mode. They use `mode: :auto` (the default), meaning they run in whatever mode Toast was started with (single server or cluster, determined by global config or CLI flag). This allows the same suite to run against both deployment modes. Suites that have mode-specific requirements (e.g., resilience tests that only make sense in cluster mode) can override: `mode: :cluster` or `mode: :single_server`.

**`:auto` resolution**: The runner resolves `:auto` to an actual mode (`:single_server` or `:cluster`) before calling `Toast.Deployment.start/2`. The deployment API itself only accepts `:single_server` or `:cluster` — it never sees `:auto`. Resolution uses the global mode from `Toast.Config` (set via CLI flag `--cluster`/`--single`, env var, or `.toast.local.exs`).

**Server arguments**: Suites can specify additional CLI arguments for the arangod processes via `server_args` (applied to all servers) or role-specific `coordinator_args`, `dbserver_args`, `agent_args` (applied only to that role). These are merged with the global defaults. Role-specific args take precedence over `server_args` for that role. This replaces the need for per-suite environment variable manipulation.

```elixir
use ToastTest.Suite,
  server_args: ["--log.level", "debug"],              # All servers
  coordinator_args: ["--javascript.enabled", "true"]   # Coordinators only
```

Test modules then use the suite:

```elixir
defmodule ShellServer.CollectionTest do
  use ShellServer.Suite

  test "create collection", %{client: client} do
    # ...
  end
end
```

The `use ToastTest.Suite` macro turns the suite module itself into a CaseTemplate. It does NOT generate a separate module — the suite module IS the CaseTemplate that test modules `use`. Specifically, the macro:
1. Injects `use ExUnit.CaseTemplate` and a `__using__` callback into the suite module. The `__using__` callback delegates to `ToastTest.Case` internally, which provides the base test context (`%{deployment: ..., client: ..., endpoint: ...}`).
2. Injects both a `@toast_suite` module attribute AND a `def __toast_suite__` function into test modules that `use` this suite. The attribute enables compile-time checks; the function enables runtime discovery by the runner. The quoting is `@toast_suite unquote(__MODULE__)` where `__MODULE__` is evaluated in the suite module's context, not the test module's context.

For resilience suites that need custom deployment logic:

```elixir
defmodule Resilience.Suite do
  use ToastTest.Suite,
    mode: :cluster,
    cluster_dbservers: 3,
    cluster_coordinators: 2

  def setup_deployment(deployment) do
    # Custom cluster validation, etc.
    {:ok, %{deployment: deployment}}
  end
end
```

#### Suite Discovery and Execution

The `mix toast` task discovers suites and orchestrates execution:

0. Call `ExUnit.start(autorun: false)` early in the task, before any suite compilation. This must happen before modules that `use ExUnit.CaseTemplate` are compiled, as ExUnit's application env must be initialized. The `autorun: false` flag prevents ExUnit from running tests on its own — the custom runner manages execution.
1. Compile all `suite.ex` files in `suites/` subdirectories
2. Scan loaded modules for the `ToastTest.Suite` behaviour (via behaviour introspection)
3. For each requested suite (or all if none specified), **sequentially**:
   0. Compile `.ex` helper files and `test_*.exs` test files for this suite
   a. Read the suite's `deployment_config/0`
   b. Start the deployment with explicit keyword opts (not env vars)
   c. Run all test modules with matching `__toast_suite__/0` function (runtime query)
   d. Collect diagnostics and stop the deployment
   e. Clean up inter-suite state (see Section 4.8)
4. Aggregate results across all suites

**Compilation constraint**: Suite modules (`suite.ex`) must not depend on helper modules (`*.ex`) within the same folder. Suites are compiled first globally; helpers are compiled per-suite before test loading.

**Module namespace requirement**: All suite-local modules — both helper modules (`.ex` files) AND test modules (`test_*.exs` files) — MUST be namespaced under the suite's top-level namespace. For example, `suites/smoke/helpers.ex` should define `Smoke.Helpers`, not bare `Helpers`, and `suites/smoke/test_version.exs` should define `Smoke.VersionTest`, not bare `VersionTest`. If two suites define the same top-level module name, the second compilation silently replaces the first in the BEAM. The suite discovery step validates this: after compiling a suite's modules, verify all newly defined modules are namespaced under the suite. Emit a warning for violations.

**Suite compilation failure handling**: A compile error in one `suite.ex` or its helpers must not prevent other suites from running. The mix task wraps per-suite compilation in error handling. On `CompileError` or `Code.LoadError`, the suite is marked as errored (all its tests get `:errored` status with the compile error message), and the runner proceeds to the next suite.

**ExUnit.Server bypass**: The runner does NOT use `ExUnit.Server` for module scheduling. ExUnit.Server is a singleton that cannot "unload" modules between suites. Instead, the runner compiles test modules per-suite and drives execution directly using ExUnit's per-test execution internals (test spawning, capture, assertion collection via `module.__ex_unit__()`). This gives the runner full control over which modules run in which suite without fighting ExUnit.Server's module registration.

**Note on ExUnit.Server accumulation**: Since test modules `use ExUnit.CaseTemplate` (via the suite CaseTemplate), they automatically register with `ExUnit.Server.add_module/2` during compilation. This is harmless — the runner never reads from ExUnit.Server (`take_async_modules`/`take_sync_modules` are never called). Modules accumulate in ExUnit.Server as dead state. `ExUnit.start()` must still be called once to initialize the ExUnit infrastructure. `modules_loaded/1` should NOT be called — the runner manages module lifecycle directly.

Suites run **sequentially, one at a time**. This ensures resource isolation (ports, memory, CPU), avoids port conflicts, and keeps the implementation simple. Parallel suite execution is a future optimization if needed.

**No async test execution**: Tests within a suite always run synchronously (never `async: true`). Running multiple tests concurrently against the same deployment risks inter-test interference. The runner enforces this — async modules are rejected with an error. This simplifies the runner significantly by removing all async scheduling logic.

#### CLI Interface

The CLI follows a path-based approach inspired by `mix test`, where arguments are paths into the `suites/` directory. Suite names are shorthand for their folder path.

```
mix toast smoke                                # Run all tests in suites/smoke/
mix toast smoke shell_server                   # Run multiple suites (space-separated, like mix test)
mix toast smoke/test_version.exs               # Run specific file within a suite
mix toast smoke/test_version.exs:42            # Run test at specific line
mix toast --cluster                            # Run all suites in cluster mode
mix toast smoke --single                       # Run smoke suite in single server mode
mix toast smoke --test "version endpoint"      # Filter by test name pattern
mix toast                                      # Run all suites
```

Multiple paths are space-separated, matching the `mix test` convention (`mix test test/foo_test.exs test/bar_test.exs`). A suite name like `smoke` is equivalent to `smoke/` — meaning "all tests in this path". A path that includes a filename like `smoke/test_version.exs` implicitly selects the `smoke` suite but only runs that file. This eliminates the need for separate `--file` and `--suite` flags.

**Mode flags**: `--cluster` and `--single` instead of `--mode <value>` for brevity. These override the global deployment mode (from config/env) for this run.

#### Folder Structure and File Conventions

```
suites/
  smoke/
    suite.ex                  # Suite definition module
    helpers.ex                # Shared helper module (compiled, not run as test)
    test_version.exs          # Test modules (must start with test_)
    test_collection.exs
  shell_server/
    suite.ex
    crud_helpers.ex           # Suite-specific helpers
    test_collection.exs
    test_document.exs
    test_aql.exs
    test_index.exs
  resilience/
    suite.ex                  # Custom deployment logic
    test_server_failure.exs
    test_coordinator_failover.exs
    test_agency_recovery.exs
```

**File naming conventions**:
- `suite.ex` — Suite definition. Exactly one per suite folder. Named `suite.ex` (not `_suite.ex`) to follow Elixir conventions where leading underscores signify unused bindings. The module name matches the convention: `Smoke.Suite`, `ShellServer.Suite`, etc.
- `test_*.exs` — Test files. Must start with `test_` and end in `.exs`. Only these files are treated as test modules by the runner. The `test_` prefix (instead of ExUnit's default `*_test.exs` suffix) groups all test files together in directory listings, making it easy to visually distinguish tests from `suite.ex` and helper files.
- `*.ex` — Helper/support modules (excluding `suite.ex`). Compiled and available to test modules but not treated as tests. This is where shared setup functions, custom assertions, domain-specific client extensions, etc. live.

This convention avoids the need for complex discovery logic: the runner compiles `suite.ex` first, then `*.ex` helpers, then loads `test_*.exs` files. No ambiguity about which files are tests vs. helpers.

#### Why This Approach

- **Clear ownership**: Each test module belongs to exactly one suite (via `@toast_suite` attribute injected by `use`)
- **Deployment control**: Suites declare requirements; framework handles lifecycle
- **Custom logic**: Suites with special needs implement optional callbacks
- **Discovery**: Folder convention + behaviour module = easy to find and validate
- **Composability**: The suite definition is just a module — can be shared, parameterized, etc.
- **No manifest files**: No YAML/JSON config to maintain; code IS the configuration
- **Mode-agnostic by default**: Most suites work with whatever deployment mode is configured globally

### 4.2 Runner Refactoring

The existing `Toast.Runner` (renamed to `ToastTest.Runner`) is an ~870-line custom fork of ExUnit.Runner with abort support, timeout clamping, async scheduling, and ExUnit.Server integration. The refactoring replaces the outer scheduling layer while preserving the per-test execution internals.

**Remove async support**: All tests run synchronously. The runner rejects modules with `async: true` at load time with a clear error. This eliminates `async_loop`, `do_async_loop`, `wait_until_available`, `spawn_modules`, async drain functions, and concurrent module tracking. Tests against a shared deployment must be sequential to avoid interference.

**Remove test module shuffling**: The existing runner supports seed-based shuffling of test module execution order. This is removed — tests run in a deterministic order (compilation order within a suite). Shuffling added complexity to discover hidden inter-test dependencies, but with the suite system providing explicit deployment lifecycle per suite, inter-test dependencies are already prevented by design.

**Replace ExUnit.Server module feeding**: The current runner receives modules from `ExUnit.Server.take_async_modules/1` and `ExUnit.Server.take_sync_modules/0`. The new runner bypasses ExUnit.Server entirely — it receives pre-compiled, pre-filtered module lists directly from the suite orchestrator. This is necessary because ExUnit.Server is a singleton with no "unload" capability, making per-suite module batching impossible through the standard path.

**Suite orchestration**:
- **Per-suite execution**: Run tests grouped by suite, with deployment lifecycle between groups
- **Suite-level timeout**: Each suite has its own timeout (default: 1 hour, configurable via `timeout:` in `use ToastTest.Suite`), clamped to the remaining global deadline
- **Suite abort**: If a deployment crashes, only that suite's remaining tests are skipped (not the entire run). The runner proceeds to the next suite.
- **Cross-suite results**: Aggregate results from all suites into a single report

**Timeout hierarchy**: Global deadline (hard wall-clock limit for the entire run, via `--global-timeout` CLI flag) > suite timeout (per-suite, clamped to remaining global time) > test timeout (per-test, via ExUnit's `@tag timeout:`, clamped to remaining suite time). If the global deadline is reached mid-suite, the current test is aborted and remaining tests/suites are skipped.

The runner becomes the orchestrator: for each suite, it compiles suite files, starts the deployment, drives test execution directly using ExUnit's per-test machinery (test spawning, capture, assertion collection), collects results, and tears down. The custom runner logic (abort support, timeout clamping) applies within each suite.

**Scope acknowledgment**: This is a substantial rewrite of the runner's scheduling layer, not a minor simplification. The per-test execution internals (test spawning, capture_log, on_exit callbacks, assertion collection) are preserved, but the outer loop that feeds modules and manages lifecycle is replaced entirely.

**ExUnit compatibility adapter**: Create a `ToastTest.ExUnitCompat` module that wraps every ExUnit internal API call: `ExUnit.RunnerStats`, `ExUnit.EventManager`, `module.__ex_unit__()`, and any other undocumented APIs. The adapter isolates version-specific assumptions into a single module. Add a compile-time check that verifies the Elixir version is within the supported range. This is the single highest-leverage action for maintainability — each Elixir version bump only requires updating the adapter, not hunting through the runner for broken internal API calls.

**Cross-suite stats aggregation**: Each suite gets its own `ExUnit.EventManager` and `ExUnit.RunnerStats` (accessed through the `ToastTest.ExUnitCompat` adapter). After each suite completes, the runner extracts stats (test count, failure count, duration) and merges them into a cross-suite accumulator. The final report aggregates stats across all suites. Formatters are attached per-suite and reset between suites (Section 4.8).

**Deployment failure handling**: If a suite's deployment fails to start, all tests in that suite are marked as `:errored` with the failure reason. The runner logs the error and proceeds to the next suite. For partially-started deployments (e.g., some servers started but health checks failed), `stop_and_collect/1` must handle gracefully — only collect diagnostics from servers that actually started, skip servers that never launched.

### 4.3 Crash Monitor

The crash monitor is extracted from the deployment library into `ToastTest.CrashMonitor`. It is provided as the `:on_crash` callback when the runner starts a suite's deployment:

```elixir
Toast.Deployment.start(:cluster,
  on_crash: &ToastTest.CrashMonitor.handle_crash/2
)
```

This module calls `ToastTest.Runner.abort!/1` to stop the current suite's tests. This breaks the coupling: `lib/toast/` knows nothing about ExUnit or runners.

### 4.4 Test Case Template

The `ToastTest.Case` module is the base CaseTemplate used internally by suite-generated CaseTemplates. It provides test context:

- `%{deployment: deployment}` — The deployment handle, enabling direct server control via `Toast.Deployment` functions
- `%{endpoint: endpoint}` — The primary endpoint URL
- `%{client: client}` — A `Toast.Client` struct for REST API calls (unauthenticated by default, see Section 5)
- Any extra context from the suite's `setup_deployment/1` callback (merged on top)

**Deployment handle delivery**: The runner stores the active deployment in an ETS-based registry keyed by suite module before running a suite's tests. `ToastTest.Case.setup` reads the deployment from this registry using the test module's `@toast_suite` attribute as the lookup key. This replaces the previous `Application.put_env(:toast, :__test_deployment__, deployment)` approach, which was a concurrency hazard: ExUnit's `on_exit` callbacks run in spawned processes and could read a stale deployment if the next suite's deployment was already stored. The ETS registry eliminates this global mutable channel. The inter-suite cleanup in Section 4.8 clears the registry entries between suites.

For resilience tests, having `deployment` in context is critical — tests call `Toast.Deployment.stop_server(deployment, "dbserver-1")` directly.

**Health check between tests**: Configurable per suite via the `between_tests` option or callback. The default checks deployment health via `Toast.Deployment.status/1`: only `:ready` is acceptable. If status is `:failed` (unexpected crash), the suite aborts. If status is `:degraded` (servers intentionally down from a previous test), the suite also aborts — **it is the test's responsibility to restore the deployment to a healthy state before finishing**.

Suites configure this via `use ToastTest.Suite`:
- Default (no option): check deployment status is `:ready`
- `between_tests: false`: disable the check entirely (fast suites with no server manipulation)
- Implement `between_tests/2` callback: custom logic for suites with specific needs

```elixir
@callback between_tests(Toast.Deployment.t(), ExUnit.Test.t()) :: :ok | {:error, term()}
@optional_callbacks [between_tests: 2]
```

The runner calls the suite's `between_tests/2` if exported, otherwise uses the default health check, unless `between_tests: false` was set. The clear error message when `:degraded` is detected: "Deployment is degraded after test X — servers [list] are still down. Tests must restore all servers before finishing."

**Migration path**: During the transition, the existing `Toast.TestCase` module is preserved as a thin wrapper that delegates to `ToastTest.Case`. It emits a compilation warning suggesting migration to suite-based test modules.

### 4.5 Process History and Diagnostics Attribution

Tests may start and stop multiple deployments over the course of a suite (especially resilience tests that restart servers). Crash and sanitizer reports are generated per-process and need to be attributed to the correct server instance after the fact. This requires maintaining a history of all processes started during test execution.

**Deployment Event Observer**: The deployment accepts an optional `:on_event` callback when started (alongside `:on_crash`):

```elixir
Toast.Deployment.start(:cluster,
  on_crash: &ToastTest.CrashMonitor.handle_crash/2,
  on_event: &ToastTest.ProcessHistory.handle_event/1
)
```

The callback must be **non-blocking** (fire-and-forget). For `SingleServerController`, the callback is invoked directly from the GenServer process. For `ClusterController`, server starts happen in spawned tasks — these tasks call the `on_event` callback directly (since it is non-blocking by contract). The controller also calls `on_event` for events it generates itself (e.g., unexpected crashes detected via erlexec `:DOWN` messages). The `ProcessHistory` implementation uses `GenServer.cast` internally, ensuring neither the controller nor the launch tasks are blocked by event processing. The `ProcessHistory` GenServer handles event ordering via timestamps, so the lack of serialization through the controller is not a problem.

The callback receives tuples:
- `{:server_started, server_id, os_pid, timestamp}` — A new server process was started
- `{:server_stopped, server_id, os_pid, exit_info, timestamp}` — A server process exited
- `{:server_crashed, server_id, os_pid, crash_info, timestamp}` — A server crashed unexpectedly

The test framework registers a `ToastTest.ProcessHistory` GenServer as the consumer:

The observer maintains a log of all process lifecycle events, keyed by OS PID and timestamped. During diagnostics collection (`stop_and_collect/1`), the crash matcher and sanitizer matcher use this history to correlate:
- Sanitizer log files (named with OS PID) → specific server instance → test that was running at that time
- Core dumps (named with OS PID) → specific server instance

The observer lives in `lib/toast_test/` because it is test-specific — IEx sessions and scripts don't need diagnostics attribution. The deployment library provides the event hook; the test framework provides the consumer.

### 4.6 Server ID Mapping

In a cluster, ArangoDB assigns internal server IDs (e.g., `PRMR-abc123`) when servers join the cluster. These IDs are used in cluster-internal operations: shard leadership, replication follower lists, agency state. Some tests need to operate on a specific server based on its cluster-internal role (e.g., "crash the leader of shard X").

The deployment maintains a mapping between Toast server IDs (stable, assigned at startup: `dbserver-0`, `coordinator-1`, etc.) and cluster-internal IDs (dynamic, assigned during cluster formation via `ServerState.cpp`). The mapping is populated after cluster health check succeeds (agency is up, all servers registered). The mapping is fetched from the agency (`/_admin/cluster/health`) and cached in the ClusterController's GenServer state.

```elixir
# Query by toast ID
Toast.Deployment.cluster_id(deployment, "dbserver-0")
# → "PRMR-abc123"

# Query by cluster-internal ID
Toast.Deployment.server_by_cluster_id(deployment, "PRMR-abc123")
# → %{toast_id: "dbserver-0", role: :dbserver, ...}

# Control operations also accept cluster-internal IDs
Toast.Deployment.stop_server(deployment, cluster_id: "PRMR-abc123")
```

This mapping enables tests like:
1. Query the agency to find the leader of a specific shard
2. Use the cluster-internal ID to identify which Toast server to crash
3. Verify that leadership moves to a different server after the crash

### 4.7 Interactive Test Execution

Since the deployment infrastructure is usable from IEx, it should also be possible to run specific tests interactively against a running deployment. This enables a workflow where a developer starts a deployment once and iterates on test code rapidly without waiting for server startup each time.

```elixir
# In IEx:
{:ok, deployment} = Toast.Deployment.start(:cluster, build_dir: "/path")
ToastTest.Interactive.run(Smoke.VersionTest, deployment: deployment)
# Runs all tests in the module against the existing deployment
# Output goes to stdout, no result packaging

ToastTest.Interactive.run(Smoke.VersionTest, "test version endpoint", deployment: deployment)
# Run a single test by name
```

`ToastTest.Interactive.run/2` is a thin wrapper that:
1. If given a file path instead of a module, compiles the file first via `Code.require_file/1`
2. Sets up the test context (deployment handle, client, endpoint) as if a suite were running
3. Runs the specified module/test via ExUnit's execution machinery
4. Returns results without the full suite lifecycle (no deployment start/stop, no result export)

This is straightforward to support because the deployment and test execution are already decoupled. The interactive runner just skips deployment management and provides the existing deployment as context.

```elixir
# Run by module (must be already compiled/loaded):
ToastTest.Interactive.run(Smoke.VersionTest, deployment: deployment)

# Run by file path (compiles on the fly):
ToastTest.Interactive.run("suites/smoke/test_version.exs", deployment: deployment)

# Edit the file, recompile, and re-run:
ToastTest.Interactive.run("suites/smoke/test_version.exs", deployment: deployment)
# File is recompiled via Code.compile_file — the new module replaces the old one in the BEAM.
```

**Recompilation and ExUnit.Server**: When a test file is recompiled, `use ExUnit.CaseTemplate` triggers `ExUnit.Server.add_module/2` again. Since the runner bypasses ExUnit.Server entirely (never reads from it), duplicate registrations are harmless dead state. The fresh `module.__ex_unit__()` function reflects the newly compiled test metadata.

### 4.8 Inter-Suite State Cleanup

The runner uses a `%ToastTest.SuiteRun{}` context struct threaded through execution functions. This struct holds per-suite state (deadline, timeout_factor, results, diagnostics, matching state) and goes out of scope when the suite completes — no explicit cleanup needed for these values. This replaces the previous `Application.put_env(:toast, ...)` approach that required manually deleting 6+ keys between suites.

Between suite executions, the runner resets the remaining shared state:
- Deployment registry (ETS-based registry keyed by suite module — clear the completed suite's entry)
- ExUnit abort table (ETS table used by `Runner.abort!/1`)
- ExUnit `after_suite` callbacks — these accumulate globally via `ExUnit.after_suite/1` and are never cleared by ExUnit itself. Without explicit cleanup, callbacks registered during suite 1 would fire during suite 2's teardown. Clear via the `ToastTest.ExUnitCompat` adapter (which accesses ExUnit's internal state to reset accumulated callbacks).
- Formatter state (reset via stopping and restarting formatter GenServers)
- BEAM code purging (optional): call `:code.purge/1` for completed suites' test modules to reclaim memory. Not mandatory (test modules are small), but available if suite count grows large.
- Port allocator state: do NOT reset between suites. Continue allocating from where the allocator left off, so recently-released ports (which may still be in TCP TIME_WAIT for ~60s) are not reissued to the next suite's deployment.

### 4.9 Result Export

The existing `Toast.ResultExporter` (renamed to `ToastTest.ResultExporter`, JSON + JUnit XML) is preserved and extended:

- Suite-level grouping in results: each suite is a top-level entry with its own deployment info, diagnostics, and test results
- Global summary: aggregated pass/fail/skip counts across all suites
- Timing: per-suite and per-test durations

---

## 5. REST Client (`lib/toast/client/`)

### 5.1 Design

The client is a thin wrapper over ArangoDB's REST API, designed for test use. It uses the `Req` library for HTTP.

#### Core Module

```elixir
defmodule Toast.Client do
  @type t :: %__MODULE__{
    base_url: String.t(),
    database: String.t(),
    api_version: non_neg_integer() | String.t() | nil,
    auth: auth_t() | nil,
    req: Req.Request.t()
  }

  @type auth_t :: {:basic, String.t(), String.t()} | {:jwt, String.t()}

  # Connection
  def new(base_url, opts \\ []) :: t()

  # Scoped clients (return new client with overridden field)
  def with_database(client, database) :: t()
  def with_auth(client, auth) :: t()
  def with_api_version(client, version) :: t()

  # Raw HTTP (for extensibility)
  def get(client, path, opts \\ []) :: {:ok, map()} | {:error, term()}
  def post(client, path, body, opts \\ []) :: {:ok, map()} | {:error, term()}
  def put(client, path, body, opts \\ []) :: {:ok, map()} | {:error, term()}
  def delete(client, path, opts \\ []) :: {:ok, map()} | {:error, term()}
end
```

The `auth` field supports both basic authentication and JWT tokens. JWT token generation uses the `joken` library (added as a dependency).

#### Authentication Ergonomics

Most tests run with authentication disabled for simplicity (ArangoDB started with `--server.authentication false`). The default client provided in test context is unauthenticated. For tests that need authentication:

```elixir
# Suite-level: all tests in this suite use auth
defmodule AuthSuite do
  use ToastTest.Suite,
    server_args: ["--server.authentication", "true"]

  def setup_deployment(deployment) do
    # Client with root credentials is provided automatically when auth is enabled
    {:ok, %{}}
  end
end

# Test-level: get an authenticated client from the unauthenticated one
test "authenticated request", %{client: client, deployment: deployment} do
  auth_client = Toast.Client.with_auth(client, {:basic, "root", ""})
  # or JWT:
  jwt_client = Toast.Client.with_auth(client, {:jwt, token})
end
```

The `with_auth/2` function returns a new client with credentials set — it does not mutate the original. This makes it trivial to switch between authenticated and unauthenticated calls within a single test.

#### Client for Specific Servers

In cluster mode, the default client connects to a coordinator. For tests that need to talk to a specific server (e.g., a dbserver, an agent, or a specific coordinator):

```elixir
dbserver_client = Toast.Deployment.client(deployment, "dbserver-1")
dbserver_client = Toast.Deployment.client(deployment, role: :dbserver, index: 0)
dbserver_client = Toast.Deployment.client(deployment, cluster_id: "PRMR-abc123")
```

This creates a new `Toast.Client` with the `base_url` pointing to that server's endpoint. The client is otherwise identical (same auth, same API version). Accepts Toast server IDs, role-based targeting, and cluster-internal IDs. This function lives on `Toast.Deployment` (not `Toast.Client`) to keep the dependency direction correct: the deployment module knows about the client, not vice versa.

#### API Versioning

ArangoDB uses URL-path-based API versioning. All API paths can be prefixed with `/_arango/vX` where `X` is a non-negative integer:
- No prefix → server's default API version (v0 in 3.12, v1 in 4.0)
- `/_arango/v0` → legacy/current API
- `/_arango/v1` → ArangoDB 4.0 API (endpoints removed for 4.0 return 404)
- `/_arango/experimental` → experimental APIs
- Higher versions only contain endpoints with breaking changes from previous version

The URL construction follows the ArangoDB convention: version prefix first, then database prefix, then API path — e.g., `/_arango/v1/_db/mydb/_api/document/coll/key`.

**Two distinct use cases for API versioning in tests**:

1. **Infrastructure operations** (test setup/teardown: creating collections, inserting documents, checking status). These should "just work" regardless of which API version is being tested. The client's default API version is used, configured globally via `Toast.Config` or `.toast.local.exs`. Unversioned domain modules (`Toast.Client.Collection`, `Toast.Client.Document`, etc.) are designed for this use case — they provide stable helpers for common operations using the global default version.

2. **Test-target operations** (the specific API endpoint/behavior this test is verifying). These need an explicit version and may have version-specific parameters or behavior. Use `with_api_version/2` to create a version-pinned client for these operations. **Versioned domain modules** (e.g., `Toast.Client.V1.Document`) are deferred — they can be added later when a second API version is actively being implemented, keeping current complexity low (YAGNI).

The client supports this via the `api_version` field and `with_api_version/2`:

```elixir
# Infrastructure: uses globally configured API version (from Toast.Config)
Toast.Client.Collection.create(client, "my_collection")
# → POST /_api/collection  (or /_arango/v1/_api/collection if default is v1)

# Test-target: explicit version via with_api_version/2
v1_client = Toast.Client.with_api_version(client, 1)
Toast.Client.Document.insert(v1_client, "my_collection", %{name: "test"})
# → POST /_arango/v1/_api/document/my_collection

# Test-target: raw HTTP with explicit version
Toast.Client.get(v1_client, "/_api/collection/my_collection/properties")
# → GET /_arango/v1/_api/collection/my_collection/properties

# With database scoping — version prefix comes first
db_client = v1_client |> Toast.Client.with_database("mydb")
# → GET /_arango/v1/_db/mydb/_api/collection/my_collection/properties

# Experimental API — same function, accepts string
exp_client = Toast.Client.with_api_version(client, "experimental")
# → GET /_arango/experimental/_api/...
```

`with_api_version/2` accepts both integers (producing `/_arango/vN`) and strings (producing `/_arango/{string}`). This unifies version and prefix handling into a single function.

When `api_version` is nil (default), the globally configured default version is used. If no global default is set, no prefix is added — requests use the server's default API version.

**Global default version**: Configured via `Toast.Config` (`api_version` key), overridable via env var `TOAST_API_VERSION` or `.toast.local.exs`. This allows running the entire test suite against a specific API version without changing test code — useful for verifying that all infrastructure operations work against a new API version.

**Incomplete API version caveat**: Per ArangoDB's versioned API design, the latest API version may be "incomplete" — it only contains endpoints with breaking changes from the previous version. Setting the global default to an incomplete version would cause infrastructure operations (collection creation, etc.) to get 404 responses. The global default should always be a "complete" version (e.g., `nil` for server default, or `1` for 4.0). Only use `with_api_version/2` for test-target operations against incomplete versions.

#### Domain Modules

Domain modules provide ergonomic wrappers around common ArangoDB REST operations. They use the client's configured API version (global default or per-client override via `with_api_version/2`):

```elixir
Toast.Client.Admin       # /_api/version, /_admin/status, /_admin/statistics
Toast.Client.Collection  # /_api/collection — create, drop, list, truncate, properties
Toast.Client.Document    # /_api/document — insert, get, update, replace, remove
Toast.Client.AQL         # /_api/cursor — execute queries, explain
Toast.Client.Index       # /_api/index — create, list, drop
Toast.Client.Graph       # /_api/gharial — graph CRUD (future)
Toast.Client.Replication # /_api/replication — state, sync (future)
```

For test-target operations that need a specific API version, use `with_api_version/2` before calling domain module functions. This makes the version explicit at the call site without requiring separate versioned module hierarchies. Versioned domain modules (e.g., `Toast.Client.V1.Document`) are deferred until a second API version actually exists with different function signatures — until then, `with_api_version/2` is sufficient and avoids premature abstraction.

Each domain module takes a `Toast.Client.t()` as first argument and returns tagged tuples.

#### Extensibility

Test suites that need domain-specific client functions add them as helper modules (`.ex` files) within their suite folder. The raw `get/post/put/delete` functions on `Toast.Client` make this trivial:

```elixir
# In suites/search/view_helpers.ex:
defmodule Search.ViewHelpers do
  def create_view(client, name, opts) do
    Toast.Client.post(client, "/_api/view", Map.merge(%{name: name, type: "arangosearch"}, opts))
  end
end
```

No need for a plugin system or registry — just functions that take a client.

---

## 6. Coredump Analysis (`lib/toast/diagnostics/`)

### 6.1 Debugger Integration

A new module `Toast.Diagnostics.Coredump` handles coredump discovery and analysis. It supports both GDB and LLDB, configurable via `Toast.Config` (`.toast.local.exs` or `TOAST_DEBUGGER` env var). Default: auto-detect (prefer LLDB if available, fall back to GDB).

#### Discovery

After a server crashes, scan the work directory and system coredump locations for core files:
- `{server_dir}/core*`
- `/tmp/core*` (filtered by PID)
- System coredump path from `/proc/sys/kernel/core_pattern`
- If `core_pattern` starts with `|` (pipe to handler like `systemd-coredump` or `apport`), use `coredumpctl list --since` filtered by PID to locate core files. This is common on CI Docker images.
- `TOAST_COREDUMP_DIR` env var override: when set, search this directory in addition to the standard locations. This handles non-standard core handlers (e.g., `apport`) that store core files in custom locations.

Match core files to server instances by PID from the filename or by checking with the debugger. The debugger validates the binary match implicitly — mismatches are logged and skipped.

#### Stack Trace Extraction

Run the debugger non-interactively to extract stack traces:

**GDB**:
```
gdb -batch -ex "thread apply all bt full" -ex "quit" <arangod_binary> <core_file>
```

**LLDB**:
```
lldb -c <core_file> -o "thread backtrace all" -o "quit" -- <arangod_binary>
```

The module has a `Toast.Diagnostics.Coredump.Debugger` behaviour with two implementations (`GDB` and `LLDB`), each responsible for constructing the command and parsing the debugger-specific output format into a common struct. Both extract:
- Thread list with stack frames
- Frame filtering: remove internal glibc/libstdc++ frames, keep ArangoDB frames
- Crash location (signal, faulting address)

The module uses a process executor to run the debugger with a timeout. Debugger availability is detected at startup; if neither is available, coredump analysis is skipped with a warning and core files are still collected for the result package.

#### Lifecycle Position

Coredump analysis runs as a **post-shutdown step** in `stop_and_collect/1`. See Section 6.2 for the full lifecycle ordering, which includes agency dump (before shutdown) and coredump analysis (after shutdown).

#### Output

The coredump analysis produces:

```elixir
%Toast.Diagnostics.CoredumpReport{
  core_path: Path.t(),
  binary_path: Path.t(),
  debugger: :gdb | :lldb,
  signal: String.t(),
  faulting_address: String.t() | nil,
  threads: [%{id: integer(), frames: [%{function: String.t(), file: String.t(), line: integer()}]}],
  crash_thread: integer()
}
```

### 6.2 Agency Dump (Cluster Diagnostics)

For cluster deployments, the agency stores critical state about the cluster topology: shard leadership, replication follower lists, server registrations, and pending operations. When cluster tests fail, this information is often essential for diagnosing what went wrong (e.g., an unexpected leadership change that caused a test assertion to fail).

A new module `Toast.Diagnostics.AgencyDump` captures agency state from a living agent after a cluster test suite completes. Since agents replicate a single globally shared state, querying one agent is sufficient — there is no need to query all agents.

#### When to Capture

The agency dump is taken during `stop_and_collect/1` for cluster deployments, **before shutting down agent processes**. The dump queries the first responsive agent. If no agents are alive or responsive, the dump is skipped with a warning.

The trigger condition: the dump is captured once per suite when diagnostics are collected. In the JS framework, agency dumps happen at several points (force termination, pre-shutdown, shutdown timeout, health check failure). For Toast, the single `stop_and_collect/1` lifecycle point is sufficient — it covers the normal end-of-suite collection and abnormal abort collection.

#### Endpoints

Query the first responsive agent via three REST endpoints:

```elixir
# Agency configuration
GET /_api/agency/config → agencyConfig_{pid}.json

# Full agency state
GET /_api/agency/state → agencyState_{pid}.json

# Agency plan (the /arango tree)
POST /_api/agency/read with body [["/arango"]] → agencyPlan_{pid}.json
```

#### Configuration

Enabled by default for cluster deployments (`dump_agency_on_error: true` in `Toast.Config`). Can be disabled via config or CLI flag `--no-agency-dump`. This matches the JS framework behavior (default on, opt-out).

#### Output

```elixir
%Toast.Diagnostics.AgencyDump{
  agent_id: String.t(),
  config: map() | nil,
  state: map() | nil,
  plan: map() | nil,
  error: String.t() | nil
}
```

The dump files are included in Tier 2 of the result packaging (compressed archive alongside server logs and crash reports).

#### Lifecycle Position

In `stop_and_collect/1` for cluster deployments:
1. Agency dump from living agents (new) — must happen before agent shutdown
2. Controller shuts down all server processes (existing)
3. Diagnostics collected from logs and sanitizer files (existing)
4. Coredump analysis runs with its own timeout (new)
5. All diagnostics (including agency dump and coredump reports) returned

**Implementation**: `stop_and_collect/1` orchestrates a multi-step protocol on the controller. It first calls a dedicated `dump_agency/1` GenServer call on the controller (which queries the agency endpoints from living agents and stores the result in its state), then calls `shutdown/2` to stop all processes, then proceeds with log/sanitizer collection and coredump analysis. This keeps the diagnostics module out of the controller GenServer while ensuring the agency dump happens before shutdown.

---

## 7. Result Packaging

### 7.1 Local vs CI Runs

Result handling differs between local development and CI:

**Local runs** (`mix toast` without `--ci`): No packaging is performed. All data remains in the work directory on the local filesystem. The framework prints a summary to stdout and the path to the work directory for manual inspection. The analysis tool (`mix toast.analyze`) can read results directly from the work directory.

**CI runs** (`mix toast --ci` or `TOAST_CI=true`): Results are packaged for artifact upload. The packaging is tiered:

#### Tier 1: Always Published (Small, Always Needed)

Published directly as CI artifacts without archiving:
- `results.json` — Full structured results
- `results.xml` — JUnit XML for CI test reporting
- `toast.log` — Framework debug log

#### Tier 2: Compressed Archive (Medium, Usually Needed)

Packed into a single compressed archive `toast-logs.tar.gz`:
- Server log files (per-server)
- Sanitizer reports (ASAN/LSAN/UBSAN/TSAN log files)
- Crash reports (extracted stack traces from debugger)

#### Tier 3: Individually Compressed (Large, Rarely Needed)

Each file compressed individually (e.g., `core.12345.zst`), published as separate artifacts:
- Core dumps
- Full database directories (if `--keep-work-dir` is set)

Tier 3 artifacts are only created when they exist (crashes are not the common case). Compression uses zstd for speed and ratio; falls back to gzip if zstd is unavailable.

### 7.2 CircleCI Integration

The `mix toast` task exits with status codes:
- 0: All tests passed, no sanitizer errors
- 1: Test failures (some tests failed, but no infrastructure issues)
- 2: Infrastructure failure (deployment couldn't start, global timeout exceeded)
- 3: Crash (server crashed unexpectedly during testing)
- 4: Sanitizer errors (all tests passed, but ASAN/TSAN/UBSAN reported issues)

For mixed results across suites: the highest-severity exit code wins (3 > 2 > 4 > 1 > 0).

CircleCI configuration uploads tier 1 files directly and uses `store_test_results` with `results.xml` for test reporting. Tier 2 and 3 artifacts are uploaded via `store_artifacts`.

---

## 8. Analysis Tool

### 8.1 Mix Task

A `mix toast.analyze` task provides post-run analysis:

```
mix toast.analyze results.json              # Summary view
mix toast.analyze results.json --failures   # Detailed failure info
mix toast.analyze results.json --crashes    # Crash diagnostics
mix toast.analyze results.json --slow 10    # Slowest N tests
```

### 8.2 Analysis Modules

```elixir
Toast.Analysis.Summary       # Pass/fail counts, durations, suite breakdown
Toast.Analysis.Failures      # Failure messages, stack traces, related diagnostics
Toast.Analysis.Crashes       # Crash reports, sanitizer errors, coredump traces
Toast.Analysis.Performance   # Slowest tests, duration distribution
```

Each module reads the `results.json` structure and produces formatted output. The analysis tool is decoupled from test execution — it operates on result files only.

---

## 9. Migration Plan

### Phase 1: Project Restructure

**Goal**: Flatten umbrella to single project. No functional changes.

Steps:
1. Create new `mix.exs` at root with combined dependencies
2. Move `apps/toast/lib/` to `lib/toast/`
3. Move `apps/toast/test/` to `test/toast/`
4. Move `apps/smoke_test/test/` to `suites/smoke/` (temporary; will be refactored in Phase 3)
5. Rename `Toast.TestCase` → `ToastTest.Case`, `Toast.Runner` → `ToastTest.Runner`, etc. (keep `Toast.TestCase` as deprecated alias)
6. Update all module references and imports
7. Ensure `test/test_helper.exs` contains only `ExUnit.start(exclude: [:integration])` — NO deployment setup, NO `setup_suite!()` calls. Unit tests (`mix test`) must never start an ArangoDB server. The suite-level deployment setup lives exclusively in the `mix toast` task and runner.
8. Verify `mix toast` and `mix test` both work
9. Delete `apps/` directory

**Deliverable**: Same functionality, flat project structure, clean namespace separation.

### Phase 2: Infrastructure Library Extraction

**Goal**: Separate `toast` (library) from `toast_test` (test framework). Make deployment infrastructure usable from IEx.

Steps:
1. Move ExUnit-dependent modules to `lib/toast_test/`: Runner, Case, CLIFormatter, ResultFormatter, ResultExporter
2. Extract crash monitor from deployment module into `ToastTest.CrashMonitor`; replace hardcoded `Runner.abort!` with `:on_crash` callback
3. Add deployment event callback mechanism for process lifecycle events (observer pattern)
4. Ensure `lib/toast/` has zero ExUnit dependencies (verify with `mix xref graph`)
5. Refactor config loading: env vars read once at task startup, passed as keyword opts to deployments. Add `.toast.local.exs` support.
6. Verify IEx workflow: `iex -S mix` → `Toast.Deployment.start(:single_server, build_dir: "...")` works
7. Add `Toast.Deployment` convenience functions for interactive use (status display, server listing)

**Deliverable**: `Toast.Deployment` usable as standalone library. Test framework builds on top via callback injection.

### Phase 3a: REST Client

**Goal**: Refactor REST client into layered architecture. Independent of the suite system.

Steps:
1. Refactor `Toast.Client` into core HTTP + domain modules (Collection, Document, AQL, Index, Admin). Defer versioned domain modules (V1.Collection, etc.) until a second API version exists — use `with_api_version/2` for explicit version pinning at call sites.
2. Add `with_auth/2`, `with_database/2`, `with_api_version/2` scoping functions; add `Toast.Deployment.client/2` for server-specific clients
3. Implement URL-path-based API versioning (`/_arango/vX/` prefix construction) with globally configurable default version (`Toast.Config` / `TOAST_API_VERSION`)
4. Add JWT authentication support (`joken` dependency)
5. Update existing smoke tests to use new client API (minimal migration — just swap function calls)

**Deliverable**: Clean layered REST client usable from IEx, test suites, and scripts. Smoke tests updated to use domain modules.

### Phase 3b: Suite System + Runner Rewrite

**Goal**: Implement suite abstraction with per-suite deployments. Rewrite the runner's outer scheduling layer.

**Pre-step: Enumerate ExUnit internal dependencies.** Before starting the runner rewrite, audit the existing runner (~870 lines) and enumerate EVERY ExUnit internal API call, struct access, and undocumented dependency. This list becomes the ExUnitCompat adapter's specification. Known dependencies include: `ExUnit.EventManager`, `ExUnit.RunnerStats`, `module.__ex_unit__()`, `%ExUnit.Test{}` / `%ExUnit.TestModule{}` structs, `ExUnit.OnExitHandler`, `ExUnit.CaseTemplate.__using__/2`, `ExUnit.Callbacks`, and Application env keys under `:ex_unit`. Every item must be wrapped by the adapter.

Steps:
1. Define `ToastTest.Suite` behaviour (deployment_config with `mode: :auto` default, optional setup/teardown/between_tests callbacks)
2. Implement `use ToastTest.Suite` macro that generates CaseTemplate and injects `@toast_suite` attribute
3. **Prototype gate**: Rewrite runner outer scheduling layer with a bounded timebox. Success criteria: can compile one suite, start a deployment, run 5 tests, abort on crash, produce stats. If prototype fails → fall back to thin wrapper over ExUnit.Server that resets state between suites.
4. Refactor runner for per-suite execution with deployment lifecycle, `%ToastTest.SuiteRun{}` context struct, deployment failure handling, and configurable between-tests checks
5. Implement suite discovery in `mix toast` task (compile `suite.ex`, then `*.ex` helpers, then `test_*.exs` tests)
6. Migrate smoke tests to new suite structure
7. Implement path-based CLI: `mix toast smoke`, `mix toast smoke/test_file.exs:42`, `--cluster`/`--single`, `--test`
8. Add orphan test file detection (warn on `.exs` files not matching `test_*.exs` pattern in suite folders)
9. Implement `ToastTest.Interactive.run/2` for running tests from IEx against existing deployments

**Deliverable**: Multiple suites can run independently, each with its own deployment. Interactive test execution from IEx.

**Runner prototype gate**: The runner rewrite (step 3) is the highest-risk item in the entire plan. Before committing to the full rewrite, the prototype must demonstrate the core flow end-to-end. If the ExUnit.Server bypass proves too fragile or requires tracking too many undocumented internals, the fallback is a thin wrapper over ExUnit.Server that calls `ExUnit.Server` normally but resets its state between suites. This is less clean (relies on ExUnit.Server internals for reset) but lower risk than a full bypass.

### Phase 4: Server Control & Resilience

**Goal**: Enable tests to deliberately manipulate servers.

Steps:
1. Extend `ServerProcess` GenServer with `handle_call` clauses for `:kill` (SIGKILL via `:exec.kill/2`), `:pause` (SIGSTOP via `:exec.kill(pid, 19)`), and `:resume` (SIGCONT via `:exec.kill(pid, 18)`) — the actual signal-sending layer. Note: SIGSTOP does not trigger erlexec's exit monitoring (the process is still alive, just frozen).
2. Add `intentional` flag and signal-type awareness to server state tracking in controllers
3. Implement control operations on Deployment: `stop_server`, `kill_server`, `pause_server`, `resume_server`, `restart_server`, `start_server` — with server ID, role-based, and cluster-internal-ID targeting. These delegate to `ServerProcess` for signal delivery.
4. Implement `expect_crash` / `verify_crash` for failure-point-triggered crashes (default 30s timeout, configurable)
5. Add `:suspend` / `:resume` messages to HealthMonitor (with timer cancellation, `:suspended` status)
6. Add `:degraded` deployment status
7. Add HealthMonitor process monitoring from controllers
8. Implement cluster-internal server ID mapping (`cluster_id/2`, `server_by_cluster_id/2`)
9. Implement `ToastTest.ProcessHistory` observer for process lifecycle tracking and diagnostics attribution (deferred from Phase 3b — only needed for resilience tests with server restarts)
10. Implement failure point management (`set_failure_point`, `clear_failure_point`, `clear_all_failure_points`) with role/server targeting
11. Write resilience test suite as proof-of-concept

**Deliverable**: Tests can stop/pause/kill/restart servers without triggering crash alerts. Expected crash verification, failure point injection, and cluster-internal ID mapping supported.

### Phase 5: Diagnostics & CI

**Goal**: Coredump analysis, tiered result packaging, analysis tooling.

Steps:
1. Implement `Toast.Diagnostics.Coredump` with pluggable debugger backend (GDB/LLDB via behaviour)
2. Add debugger auto-detection and configuration via `.toast.local.exs` / `TOAST_DEBUGGER`
3. Implement `Toast.Diagnostics.AgencyDump` for cluster diagnostics (query `/_api/agency/{config,state,read}` from living agents)
4. Integrate agency dump (pre-shutdown) and coredump analysis (post-shutdown) into `stop_and_collect/1` lifecycle
5. Implement tiered result packaging: tier 1 (always published), tier 2 (compressed archive, including agency dumps), tier 3 (individually compressed large files)
6. Implement `--ci` flag / `TOAST_CI` env var to toggle packaging behavior (local vs CI)
7. Define exit code strategy (0-4 with severity ordering)
8. Implement `mix toast.analyze` with summary, failures, crashes, and performance views
8. Test with CircleCI configuration

**Deliverable**: Full diagnostic collection with GDB/LLDB support, tiered result packaging, analysis tooling.

### Phase Order and Dependencies

```
Phase 1 (Restructure)
  ↓
Phase 2 (Library Extraction)
  ↓
Phase 3a (REST Client)     ← independent, can ship early
  ↓
Phase 3b (Suite System)    ← depends on 3a for client API; highest-risk phase
  ↓
Phase 4 (Resilience)       ← includes ProcessHistory (deferred from 3b)
  ↓
Phase 5 (Diagnostics & CI)
```

Phases 1 and 2 are safe to start now. Phase 3a is independent and low-risk. Phase 3b is the highest-risk phase (runner rewrite with ExUnit.Server bypass) and includes a prototype gate with fallback. Phase 4 builds on the suite system. Phase 5 is largely independent but benefits from having suites in place.

### Future Work (Not in Scope)

- **Client tools wrappers** (arangosh, arangodump, arangorestore): Deferred until test suites actually need them. Can be added as `Toast.ClientTools` module when required.
- **SUT checkers** (post-test invariant verification): Useful for detecting leaked collections/databases/transactions. Deferred to post-Phase 5 as an enhancement to the suite teardown flow.
- **Parallel suite execution**: Sequential execution is sufficient initially. Parallelism adds complexity (port ranges, resource isolation) with limited benefit until the suite count grows large.
- **Test bucket splitting for CI**: Partition suites across CI workers. Deferred until suite count warrants it.

---

## 10. Key Design Decisions

### D1: Suite Organization — Module-Based with Folder Convention

**Decision**: Suites are defined by modules implementing a behaviour, organized in folders by convention.

**Rationale**: This is the most Elixir-idiomatic approach. Module-based suites are code, not configuration — they can be parameterized, composed, and tested. The folder convention keeps them discoverable without requiring a manifest file.

**Alternative considered**: Tag-based suites (tests declare membership via attributes). Rejected because: tags don't naturally map to "this group of tests needs THIS deployment configuration". A suite is fundamentally about deployment requirements, not test metadata.

**Alternative considered**: Manifest files (YAML/JSON declaring suite membership). Rejected because: adds maintenance burden, duplicates information that's already in the code, and is not idiomatic Elixir.

### D2: Resilience Monitoring — Implicit via Control API

**Decision**: Control operations (stop/pause/kill) automatically mark servers as "intentionally down", suppressing monitoring alerts. No manual monitoring toggle needed.

**Rationale**: This puts the complexity in the framework, not in the tests. A test that calls `stop_server(deployment, "dbserver-1")` should not also need to remember to suppress monitoring.

**Edge cases**:
- Server crashes before control operation is processed → crash treated as unexpected, control op returns `{:error, :already_crashed}`
- Server crashes during intentional shutdown (SIGSEGV in cleanup) → signal type examined; non-SIGTERM signals clear the intentional flag and trigger crash notification
- Concurrent control operations on same server → serialized by GenServer mailbox, safe

### D3: Project Layout — Single Project with Three Zones

**Decision**: `lib/` for framework, `test/` for unit tests, `suites/` for integration tests.

**Rationale**: Clear separation of concerns without the overhead of an umbrella. The custom `mix toast` task handles suite discovery, so ExUnit's default test path is irrelevant for suites. Unit tests use standard `mix test`.

### D4: Client Architecture — Thin Wrapper + Domain Modules

**Decision**: Core HTTP client with domain-specific modules (Collection, Document, AQL, etc.), extended by test suites via plain functions.

**Rationale**: Avoids building a full-featured client library (not our goal), while providing enough structure for common operations. The raw HTTP functions (`get`, `post`, etc.) make ad-hoc extensions trivial.

### D5: Library-Framework Boundary — Callback Injection

**Decision**: The deployment library accepts an `:on_crash` callback; the test framework provides the implementation that calls `Runner.abort!`.

**Rationale**: Breaks the coupling between `lib/toast/` and ExUnit. The library can be used in IEx, scripts, and other contexts without any test framework dependency.

### D6: Sequential Suite Execution, No Async Tests

**Decision**: Suites always run sequentially, one at a time. Within a suite, all tests run synchronously (no `async: true`).

**Rationale**: Running multiple tests concurrently against the same deployment risks inter-test interference — tests create collections, modify server state, and inspect global metrics. Sequential execution guarantees isolation without requiring complex per-test sandboxing. It also simplifies the runner substantially by removing all async scheduling logic.

Parallel suite execution across different deployments might be feasible on large workstations but is impractical in resource-constrained CI environments. Deferred as a future optimization.

### D7: Mode-Agnostic Suites by Default

**Decision**: Suites default to `mode: :auto`, inheriting the deployment mode from global configuration. Only suites with mode-specific requirements (e.g., resilience tests that need a cluster) override this.

**Rationale**: Most test suites (CRUD, AQL, indexing) should work identically against a single server or a cluster. Making mode a global setting rather than a per-suite property means the same suite can test both deployment modes without code changes, and CI can run the full suite matrix by varying a single flag.

### D8: Expected Crash Mechanism

**Decision**: The `expect_crash` / `verify_crash` API provides a way for tests to declare that a server will crash (e.g., due to a failure point) before the crash happens.

**Rationale**: The `kill_server` / `stop_server` API covers cases where the test directly causes a server to stop. But failure points trigger crashes indirectly — the test sets a failure point, then performs an action that hits it, causing the server to crash at an arbitrary point. Without `expect_crash`, the health monitor would treat this as an unexpected crash and abort the suite. The expect/verify pattern is explicit, verifiable, and auto-clears on timeout to prevent leaked state.

### D9: API Versioning — Unversioned Modules with Runtime Override

**Decision**: Unversioned domain modules (`Toast.Client.Collection`, etc.) using the global default API version. Tests targeting a specific API version use `with_api_version/2` to override the version for a block of requests. Versioned domain modules (e.g., `Toast.Client.V1.Document`) are deferred until a real second API version exists with different function signatures.

**Rationale**: Most test code is infrastructure — creating collections, inserting documents, checking status. This code should not change when a new API version appears. For tests that need explicit version control, `with_api_version/2` provides a clear override at the call site without requiring a parallel module hierarchy. Since new API versions are rare by design (only for breaking changes), pre-building versioned modules is YAGNI — today there is exactly one client module per domain. When a second API version actually exists with incompatible signatures, versioned modules can be added at that point.

### D10: Local Config File

**Decision**: A `.toast.local.exs` file (gitignored) provides machine-specific configuration with lower precedence than env vars and keyword opts. Skipped in CI mode.

**Rationale**: Developers have different build directories, debugger preferences, and local setups. Environment variables work but are tedious for stable per-machine settings. A local config file is set-and-forget, while env vars and keyword opts remain available for per-run overrides.

### D11: Test Data Isolation — Test Responsibility

**Decision**: The framework does NOT provide automatic data isolation between tests. Tests are responsible for cleaning up their own data (creating and dropping collections, databases, etc.).

**Rationale**: The plan enforces `:ready` deployment state between tests (server health) but deliberately does not address data state. Full database sandboxing (like Ecto's SQL Sandbox adapter) is not feasible for ArangoDB — there is no transaction-based rollback mechanism for DDL operations (collection/database creation). The practical approaches are: (1) tests clean up after themselves, (2) suites use `setup_deployment/1` to create a dedicated test database and `teardown_deployment/1` to drop it, or (3) suites use `setup` blocks to truncate collections between tests. This is an explicit design gap acknowledged for future improvement — a potential "SUT checker" that verifies no leaked collections/databases after each test could be added post-Phase 5.

---

## 11. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Custom runner complexity grows | Hard to maintain, subtle bugs | Simplified by removing async; delegate to ExUnit machinery for actual test execution |
| Resilience tests leave servers down | Next test fails with degraded deployment | Runner enforces `:ready` between tests; clear error message naming downed servers |
| Debugger not available on CI nodes | Coredump analysis fails silently | Auto-detect GDB/LLDB at startup; skip analysis with warning if missing; still collect core files |
| Suite discovery misses modules | Tests silently not run | Strict convention: only `test_*.exs` files are tests; warn on unexpected `.exs` files in suite folders |
| erlexec signal handling edge cases | Missed crashes or false positives | Dual detection (erlexec + health monitor) is already robust; intentional flag adds minimal complexity |
| CircleCI result format changes | Report upload breaks | JUnit XML is stable; keep format simple; test CI integration early |
| Env var pollution between suites | Suite B reads Suite A's config | Env vars read once at startup; suites receive explicit keyword opts |
| HealthMonitor silent death | Server health not monitored | Controller monitors HealthMonitor processes; restarts on unexpected death |
| Stale deployment struct | Tests see outdated state | Struct is a handle; all state queries go through live functions |
| Expected crash timeout leaks | Monitor stays suspended after test | Auto-clear with configurable timeout; runner verifies `:ready` state between tests |
| Runner refactoring larger than expected | Delays Phase 3 delivery | ExUnit.Server bypass is the key risk; prototype early; preserve per-test internals |
| Coredump discovery fails on CI | Core files not found | Support both filesystem and systemd-coredump/coredumpctl paths; TOAST_COREDUMP_DIR override; skip gracefully |
| Suite compilation failure | All suites blocked | Isolate per-suite compilation; error one suite, continue others |
| ExUnit.after_suite accumulation | Callbacks leak across suites | Clear accumulated callbacks between suites in runner cleanup (Section 4.8) |
| Log size from long-running suites | OOM during Tier 2 compression | Note as known limitation; consider per-suite log rotation in future |
| Partial deployment on start failure | Diagnostics collection errors | stop_and_collect handles partial state; skip unstarted servers |

---

## 12. Success Criteria

After full implementation:

1. `iex -S mix` → `Toast.Deployment.start(:cluster, build_dir: "/path")` works interactively
2. `mix toast smoke` runs smoke tests with their own deployment
3. `mix toast resilience` runs tests that stop/restart servers without false crash alerts
4. `mix toast` runs all suites sequentially, each with its own deployment
5. `mix toast smoke --cluster` overrides deployment mode for a suite
6. `mix toast smoke/test_version.exs` filters to specific test files via path
7. `mix toast --ci` produces tiered result packages for CircleCI
8. `mix toast.analyze results.json` produces useful summaries
9. `mix test` runs framework unit tests (no ArangoDB needed)
10. Tests are straightforward to write: `use MySuite`, get fixtures, call client methods
11. A deployment failure in one suite does not prevent subsequent suites from running
12. Sanitizer-only errors produce a non-zero exit code (distinguishable from test failures)
13. Tests that leave servers down are rejected with a clear error before the next test runs
14. `.toast.local.exs` allows per-developer configuration without touching Git
15. A test failure or abort within one suite does not affect subsequent suites
