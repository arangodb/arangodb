# Research Findings

## Part 1: Codebase Analysis

### Executive Summary

**Toast** is a production-ready Elixir framework for orchestrating ArangoDB server deployments during integration testing. It handles full lifecycle management (start -> health check -> test execution -> crash diagnostics -> shutdown). **Armadillo** is a planned Python/pytest successor to the legacy JavaScript framework, with a phased implementation strategy. The frameworks share core architectural patterns but represent different design philosophies: Elixir leverages OTP for concurrent process supervision, while Python emphasizes pytest integration and modular extensibility.

---

### 1. Toast Framework (Elixir) — Current State

#### Project Structure

Umbrella project with two apps:
- `apps/toast/` — Core framework library (~30 modules)
- `apps/smoke_test/` — Example test suite using Toast

Dependencies: `req` (~0.5) for HTTP, `erlexec` (~2.0) for OS process management. Requires Elixir 1.19+, OTP 28+.

#### Architecture Layers

**Layer 1: Deployment Control** (`apps/toast/lib/toast/deployment/`)
- `Deployment` — High-level API: `start(:single_server | :cluster)`, `stop()`, `stop_and_collect()`
- `SingleServerController` / `ClusterController` — GenServer lifecycle state machines
- `Factory` — Constructs launch specs (executable, args, env, directories)
- `CommandBuilder` — Generates arangod CLI flags per role
- `Health` — HTTP readiness checks + agency consensus polling
- `ServerInstance` — Runtime state struct (id, role, port, endpoint, OS PID, log path)
- `Supervisor` — DynamicSupervisor for controllers and processes

**Layer 2: Process Management** (`apps/toast/lib/toast/process/`)
- `ServerProcess` (GenServer) — Wraps erlexec; graceful shutdown: SIGTERM -> wait -> SIGKILL; crash detection via `:DOWN` messages
- `HealthMonitor` (GenServer) — Periodic HTTP health checks with configurable interval (1s) and failure threshold (3)
- `Supervisor` — DynamicSupervisor for server processes and monitors

**Layer 3: Test Execution**
- `TestCase` (ExUnit.CaseTemplate) — Provides `%{deployment, endpoint, client}` context; `setup_suite/2` starts deployment and registers `after_suite` callback
- `Runner` — Custom ExUnit runner with suite abort support; clamping per-test timeout to suite deadline
- `Client` — Thin REST API wrapper (version, AQL, collections, documents)
- `ResultFormatter` / `CLIFormatter` — Custom event listeners for test output

**Layer 4: Diagnostics** (`apps/toast/lib/toast/diagnostics/`)
- `CrashLogParser` — Streams log file, extracts signal, backtrace, FATAL lines
- `ServerLog` — Scans for assertion failures and FATAL/WARNING
- `Sanitizer` — ASAN/LSAN/UBSAN/TSAN env setup and log collection; auto-detect from build path
- `SanitizerMatcher` / `CrashMatcher` — Correlate errors to test cases by timestamp
- `Summary` — Formats diagnostics for CLI output

**Layer 5: Result Export** (`apps/toast/lib/toast/result_exporter/`)
- `ResultExporter` — Orchestrates JSON + XML writing
- `JSON` / `JUnitXML` — Format-specific builders

**Layer 6: Infrastructure**
- `Application` — Supervision tree: PortAllocator, Process.Supervisor, Deployment.Supervisor
- `Config` — Loads from `TOAST_*` env vars; precedence: opts > env > defaults
- `PortAllocator` — Dynamic TCP port allocation
- `Mix.Tasks.Toast` — `mix toast` command

#### Key Patterns

- **GenServer state machines** with explicit status transitions: `:stopped` -> `:starting` -> `:ready` -> `:stopping` -> `:stopped` (plus `:crashed`)
- **Dual crash detection**: erlexec `:DOWN` message (process exit) + HealthMonitor timeout (unresponsive but running)
- **Timeout hierarchy**: suite deadline (monotonic time) + per-test timeout + clamp: `min(test_timeout, remaining_deadline)`
- **Tagged tuple error handling**: `{:ok, result}` or `{:error, reason}` throughout
- **DynamicSupervisor** with one-for-one strategy for variable child counts

#### Test Lifecycle

```
1. Application.start() -> Supervision tree up
2. TestCase.setup_suite() -> Deployment.start(:mode)
   - Factory builds launch spec
   - ServerProcess spawns via erlexec
   - Health.wait_until_ready() polls HTTP + checks agency
   - HealthMonitor spawned for continuous polling
3. ExUnit test execution via Toast.Runner
   - Before each test: Deployment.check_health()
   - If failed -> Runner.abort!() -> remaining tests skipped
   - On crash: ServerProcess -> controller -> crash monitor -> Runner.abort!()
4. ExUnit.after_suite callback
   - Deployment.stop_and_collect()
   - CrashMatcher / SanitizerMatcher correlate to tests
   - Summary formatted to CLI
   - ResultExporter writes JSON + XML + log
```

#### What Works Well

1. Clear separation of concerns — modules don't leak abstractions
2. Explicit state transitions prevent silent failures
3. Dual crash detection (process + health) increases coverage
4. Test fixture injection — tests don't know about infrastructure
5. Diagnostic correlation — ties server state to test outcomes
6. Timeout discipline — suite deadline + per-test clamping
7. Configuration simplicity — env vars + keyword opts, no config files

---

### 2. Armadillo Framework (Python) — Previous Attempt

#### Design Philosophy

Pytest-native, modern Python (type hints, dataclasses, Pydantic), phased rollout (7 phases), CLI-first.

#### Architecture

```
armadillo/
  core/          # errors, logging, config, timeouts, process management
  instances/     # server lifecycle (ArangoServer wrapper)
  utils/         # filesystem, crypto, auth, codecs, ports
  pytest_plugin/ # fixtures (arango_single_server, arango_cluster), markers
  results/       # collection, export (JSON, JUnit XML)
  cli/           # typer-based CLI (test run, analyze, config)
```

Key components: TimeoutManager (layered deadlines), ProcessSupervisor (escalation SIGTERM->SIGKILL), ArangoServer, pytest fixtures (package-scoped deployments).

#### Lessons for Toast

- Package-scoped deployments (one deployment per test directory) is a good model
- Timeout layering approach (per-test, global, output-idle) covers more failure modes
- Analysis CLI as first-class component (not afterthought)
- Environment probe for sanitizer detection at startup

---

### 3. Legacy JavaScript Framework Analysis

Key documents from `tests/python/plan/`:

**From `testing.js.md`:**
- Dynamic test discovery with wildcard patterns
- Option inheritance via module registration
- Bucket splitting for parallel CI runs
- Auto mode for test suite discovery + execution
- Result aggregation/normalization across suites

**From `instance.js.md` / `instance-manager.js.md`:**
- Server role-based topology (agents, dbservers, coordinators) with startup ordering
- Agency consensus polling
- Hot restart scenarios (kill + restart for resilience testing)
- Leader failover detection

**From `crash-utils.js.md`:**
- GDB integration for non-interactive crash analysis
- Core dump discovery with glob patterns
- Sanitizer log correlation with test UUIDs
- Stack frame filtering (internal frames pruned)

**From `result-processing.js.md`:**
- Multi-format export (XML, JSON, YAML)
- Performance profiling (latency, memory, network)
- Historical baseline comparison

**Gap Analysis highlights:**
- Config precedence (Toast implements this)
- Port allocator + deterministic seed (Toast implements this)
- EnvironmentProbe for sanitizer detection (Toast implements this)
- Missing bucket splitting for CI parallelization
- Core dump symbolic filtering (advanced feature)
- Confidence scoring pre-remediation: 7.2/10

---

### 4. Cross-Framework Comparison

| Capability | Toast (Elixir) | Armadillo (Python) | JS Framework |
|---|---|---|---|
| Single Server | Yes | Phase 1 | Yes |
| Cluster | Yes | Phase 2 | Yes |
| Health Checks | HTTP + Agency | HTTP | HTTP |
| Crash Detection | erlexec + health | ProcessSupervisor + health | Process exit |
| Crash Analysis | Log parsing | Phase 4 | GDB + log parsing |
| Sanitizer | ASAN/LSAN/UBSAN/TSAN | Phase 4 | ASAN/TSAN |
| Result Export | JSON + XML | JSON + XML | XML + JSON + YAML |
| Timeout Mgmt | Suite + per-test | Suite + per-test + idle | Per-suite |
| Parallel Execution | No | Phase 3 | Bucket splitting |
| Test Filtering | ExUnit tags | pytest markers | Custom filters |

---

## Part 2: Web Research — Elixir/OTP, ExUnit, erlexec, System Testing

### 1. Elixir OTP Supervision Trees

#### DynamicSupervisor for Variable Process Counts

Use `DynamicSupervisor` with `:one_for_one` strategy (the only supported strategy) for managing variable numbers of database server processes. Start with no children, add via `start_child/2`.

For a test framework, use `:temporary` restart strategy on child specs — crashed servers during testing are diagnostic events, not something to silently recover from.

Key config: `:max_children` caps concurrent children; `:max_restarts` / `:max_seconds` controls throttling (default: 3 restarts in 5 seconds).

Sources: [DynamicSupervisor docs](https://hexdocs.pm/elixir/DynamicSupervisor.html)

#### GenServer Lifecycle Patterns

Use `handle_continue/2` for multi-step initialization (start process -> wait for health -> mark ready). Guaranteed to execute before any other messages arrive.

**Critical**: `terminate/2` is NOT guaranteed to run if the GenServer does not trap exits. For reliable cleanup of external processes, use erlexec's process linking or a separate monitoring process. Elixir docs recommend: "important clean-up rules to happen in separated processes either by use of monitoring or by links themselves."

Sources: [GenServer docs](https://hexdocs.pm/elixir/GenServer.html)

#### Registry for Named Lookups

Use Elixir's `Registry` with `:via` tuples to name processes by meaningful identifiers (role, port). Auto-deregisters on process exit.

```elixir
{:via, Registry, {Toast.ServerRegistry, server_id}}
```

Sources: [Registry docs](https://hexdocs.pm/elixir/Registry.html)

#### Recommended Supervision Tree

```
Application
├── Toast.ServerRegistry (Registry, keys: :unique)
├── Toast.ServerSupervisor (DynamicSupervisor)
│   ├── Toast.Server (GenServer, temporary)
│   └── ...
├── Toast.TestOrchestrator (GenServer)
└── Toast.DiagnosticsCollector (GenServer)
```

---

### 2. ExUnit Extensions & Custom Runners

#### Key Configuration

| Option | Default | Purpose |
|---|---|---|
| `:timeout` | 60,000ms | Per-test timeout |
| `:max_failures` | `:infinity` | Abort after N failures |
| `:max_cases` | `schedulers * 2` | Max parallel test modules |
| `:formatters` | `[ExUnit.CLIFormatter]` | Output formatters |
| `:seed` | random | Test ordering |
| `:capture_log` | false | Capture Logger per test |

#### CaseTemplate Lifecycle

- `setup_all`: runs once per module, before any test. Returned context available to all tests.
- `setup`: runs per test in new process. Has access to `setup_all` context.
- `on_exit` in `setup_all`: runs after ALL tests, in reverse order, in dedicated process.
- `on_exit` in `setup`: runs after each test, before next test starts.

#### Custom Formatters

GenServers receiving cast messages: `{:suite_started, _}`, `{:test_started, _}`, `{:test_finished, _}`, `{:suite_finished, _}`, `{:module_started, _}`, `{:module_finished, _}`, `{:sigquit, _, _}`, `:max_failures_reached`.

Register via `ExUnit.configure(formatters: [ExUnit.CLIFormatter, Toast.JUnitFormatter])`.

Reference: [junit-formatter](https://github.com/victorolinasc/junit-formatter)

#### Suite Abort

`:max_failures` is the primary mechanism. Note: "It is aborted after the longest running concurrent test case finishes" — does not kill in-flight tests immediately.

For programmatic abort (e.g., server crash), no clean built-in API exists. The practical approach: custom formatter that detects critical conditions and calls `System.halt/1`, or (as Toast does) a custom runner.

#### Tags and Filtering

Tag precedence: `@tag` > `@describetag` > `@moduletag`. Reserved: `:timeout`, `:skip`, `:tmp_dir`, `:capture_log`.

#### Parameterized Tests (Elixir 1.18+)

```elixir
use ExUnit.Case, parameterize: [%{deployment: :single}, %{deployment: :cluster}]
```

Sources: [ExUnit docs](https://hexdocs.pm/ex_unit/ExUnit.html), [ExUnit.CaseTemplate](https://hexdocs.pm/ex_unit/ExUnit.CaseTemplate.html)

---

### 3. erlexec OS Process Management

#### Why erlexec

| Feature | Elixir Port | MuonTrap | erlexec |
|---|---|---|---|
| Orphan cleanup on VM crash | No | Yes (cgroups) | Yes (C++ port) |
| Signal sending | No | Limited | Full |
| Exit signal detection | Exit code only | Exit code | Signal + core dump |
| Separate stdout/stderr | No | No | Yes |
| Process groups | No | Via cgroups | Native |
| Monitor existing PIDs | No | No | Yes |

erlexec is the clear choice for signal-level control needed to detect crashes (SIGSEGV, SIGABRT) and process group management for clean teardown.

#### Starting Processes

```elixir
{:ok, pid, os_pid} = :exec.run_link(command, [
  :monitor, :kill_group, {:group, 0}, {:kill_timeout, 10},
  :stdout, :stderr, {:cd, workdir}, {:env, env_vars}
])
```

#### Crash Detection via Signal Inspection

```elixir
def handle_info({:DOWN, os_pid, :process, pid, {:exit_status, status}}, state) do
  case :exec.status(status) do
    {:status, exit_code} -> # Normal exit with code
    {:signal, signal_num, core_dump?} -> # Crash! signal_num identifies cause
  end
end
```

Common signals: SIGSEGV (11), SIGABRT (6), SIGBUS (7), SIGFPE (8), SIGKILL (9), SIGTERM (15).

#### Process Groups

Create group with first process, add subsequent processes to same group. On group leader exit, all members receive SIGTERM.

#### Command Modes

List command (e.g., `["/usr/bin/arangod", "--config", path]`) is recommended — passed directly to `execve(3)`, no shell involved. String command goes through `$SHELL`.

Sources: [erlexec GitHub](https://github.com/saleyn/erlexec), [erlexec API](https://hexdocs.pm/erlexec/exec.html)

---

### 4. System Testing Best Practices

#### Test Isolation

Shared server per module with per-test database isolation is recommended for databases with significant startup time. Start in `setup_all`, create fresh DB per test in `setup`, drop in `on_exit`.

#### Health Checks

Poll with exponential backoff: initial 100ms, double each attempt, cap at 2s. Set max attempts or total timeout. Distinguish "not ready yet" from "crashed".

#### Port Allocation

Dynamic allocation via Agent or atomic counter. Alternatives: OS-assigned (port 0), hash-based with worker ID, large range reservation.

#### Timeout Strategy

Three levels: per-test (ExUnit `:timeout` tag), suite-level (watchdog GenServer with `System.halt/1`), idle detection (GenServer timeout mechanism).

#### Sanitizer Integration

Set env vars via erlexec's `{:env, [...]}`: `ASAN_OPTIONS` with `log_path`, `detect_leaks`, `abort_on_error`, custom `exitcode` for detection. After tests, scan work directory for sanitizer log files. No standardized parser exists — parse manually.

Sources: [Sanitizer common flags](https://github.com/google/sanitizers/wiki/SanitizerCommonFlags)

#### JUnit XML

De facto CI standard. Key elements: `<testsuites>` (root), `<testsuite>` (per module), `<testcase>` (per test), `<failure>`/`<error>`/`<skipped>`, `<properties>`, `<system-out>`/`<system-err>`.

#### CI Integration

- Non-zero exit for failures; distinguish test failures (1) from infrastructure failures (2) from timeouts (3)
- Upload work directories with logs, core dumps, sanitizer reports as CI artifacts
- Set suite-level timeouts below CI timeouts for graceful cleanup
- Random seed by default; seed 0 for reproducibility

---

### Testing Patterns in Toast

Toast uses ExUnit with:
- `Toast.TestCase` as CaseTemplate providing deployment fixtures
- `Toast.Runner` as custom runner for abort support and timeout clamping
- `Toast.CLIFormatter` and `Toast.ResultFormatter` as custom formatters
- Standard ExUnit tags (`:single_only`, `:cluster_only`) for test filtering
- `setup_suite` / `after_suite` for deployment lifecycle
- `on_exit` for per-test cleanup
