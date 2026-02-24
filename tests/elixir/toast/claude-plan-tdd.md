# Toast TDD Plan

Tests to write BEFORE implementing each section of the implementation plan. All tests use ExUnit and live in `test/` (unit tests for the framework). Integration-level testing is done by running the actual suites against a real ArangoDB deployment.

Testing conventions:
- Framework: ExUnit
- Unit tests: `test/toast/` and `test/toast_test/`
- Mocking: Mox for behaviour-based mocks, or simple module stubs
- External dependencies mocked: erlexec (OS processes), HTTP (Req), filesystem (for coredump discovery)
- Tests run with `mix test` (no ArangoDB needed)

---

## 2. Project Structure

No tests needed — this is a structural reorganization (moving files, renaming modules). Verified by: `mix compile` succeeds, `mix toast` still works, `mix test` still works.

---

## 3. Infrastructure Library (`lib/toast/`)

### 3.1 Deployment API

```elixir
# Test: Deployment struct contains immutable fields (id, mode, config, controller, endpoint, work_dir)
# Test: Deployment.start/2 returns {:ok, deployment} with :single_server mode
# Test: Deployment.start/2 returns {:ok, deployment} with :cluster mode
# Test: Deployment.start/2 returns {:error, reason} on invalid config
# Test: Deployment.start/2 rejects :auto mode (only :single_server | :cluster accepted)
# Test: Deployment.stop/1 returns :ok
# Test: Deployment.stop_and_collect/1 returns diagnostics map or nil
# Test: Deployment.status/1 returns live status from controller
# Test: Deployment.server/2 returns server state for valid server_id
# Test: Deployment.servers/1 returns all servers
# Test: Deployment.servers/2 filters by role
# Test: Deployment.endpoint/1 returns the primary endpoint
# Test: For cluster, endpoint is first coordinator's URL
# Test: Deployment.client/2 returns client for specific server by toast ID
# Test: Deployment.client/2 returns client for server by role/index
# Test: Deployment.client/2 returns client for server by cluster_id
# Test: Deployment.client/2 returns error for unknown server
```

#### Server Control Operations

```elixir
# Test: stop_server/2 returns :ok and marks server as intentionally stopped
# Test: stop_server/2 returns {:error, :already_crashed} if server already crashed
# Test: kill_server/2 sends SIGKILL via :exec.kill/2 and marks as intentionally killed
# Test: pause_server/2 sends SIGSTOP via :exec.kill(pid, 19) and marks as paused
# Test: resume_server/2 sends SIGCONT via :exec.kill(pid, 18) and resumes monitoring
# Test: restart_server/2 stops then starts server
# Test: restart_server/3 with args: [...] merges additional CLI arguments with original launch spec
# Test: restart_server preserves immutable properties (port, data directory, binary)
# Test: start_server/2 starts a previously stopped server
# Test: expect_crash/2 returns {:ok, ref} and suspends monitoring
# Test: expect_crash/3 with timeout: option overrides default 30s
# Test: verify_crash/2 returns {:ok, crash_info} when server crashed as expected
# Test: verify_crash/2 returns {:error, :not_crashed} when server still running
# Test: verify_crash/2 returns {:error, :timeout} after timeout expires
# Test: expect_crash auto-clears after timeout and resumes monitoring
# Test: late crash after timeout clearing treated as unexpected (runner health check catches it)
# Test: role-based targeting — stop_server(deployment, role: :dbserver) stops all dbservers
# Test: role-based targeting — pause_server(deployment, role: :coordinator, index: 0)
# Test: cluster_id targeting — stop_server(deployment, cluster_id: "PRMR-xxx")
```

#### Crash Notification

```elixir
# Test: on_crash callback is invoked on unexpected crash
# Test: on_crash callback is NOT invoked on intentional stop
# Test: on_crash callback is NOT invoked when expect_crash is set
# Test: no crash callback when none provided (IEx mode)
# Test: SIGSEGV during intentional shutdown triggers on_crash (signal-type awareness)
# Test: SIGTERM during intentional shutdown is treated as intentional
```

#### Event Observer

```elixir
# Test: on_event callback fires for :server_started with server_id, os_pid, timestamp
# Test: on_event callback fires for :server_stopped
# Test: on_event callback fires for :server_crashed
# Test: on_event is non-blocking (GenServer.cast path)
# Test: ClusterController tasks send events back to controller which forwards to on_event
# Test: no event callback when none provided
```

### 3.2 Controller Architecture

```elixir
# Test: server state transitions — :running → stop_server → :stopped (intentional: true)
# Test: server state transitions — :running → unexpected crash → :crashed (intentional: false)
# Test: server state transitions — :paused → resume → :running
# Test: server state transitions — :stopped → start_server → :running
# Test: SIGSEGV during intentional stop clears intentional flag
# Test: race condition — crash before stop_server returns {:error, :already_crashed}
# Test: ClusterController deployment status: all running → :ready
# Test: ClusterController deployment status: some intentionally down → :degraded
# Test: ClusterController deployment status: unexpected crash → :failed
# Test: Controller monitors HealthMonitor; restarts on unexpected death
```

### 3.3 Health Monitor Updates

```elixir
# Test: HealthMonitor accepts :suspend message and stops polling
# Test: HealthMonitor :suspend cancels pending Process.send_after timer
# Test: HealthMonitor accepts :resume message and restarts polling
# Test: HealthMonitor.status/1 returns :healthy | :unhealthy | :suspended
# Test: suspended monitor does not fire :check messages
# Test: multiple :suspend then single :resume restores monitoring
```

### 3.4 Application Supervision Tree

```elixir
# Test: Toast.Application starts PortAllocator, Process.Supervisor, Deployment.Supervisor
# Test: all child processes use :temporary restart strategy
```

### 3.5 Failure Point Management

```elixir
# Test: set_failure_point/3 calls /_admin/debug/failat/{name} on target server
# Test: clear_failure_point/3 removes specific failure point
# Test: clear_all_failure_points/1 clears all on all servers
# Test: role-based targeting for failure points
# Test: returns {:error, :not_supported} when server doesn't support failure points
```

### 3.6 Configuration

```elixir
# Test: config precedence — keyword opts > env vars > .toast.local.exs > defaults
# Test: .toast.local.exs is read at startup if present
# Test: .toast.local.exs is ignored if absent (no error)
# Test: global api_version configurable via Toast.Config
# Test: TOAST_API_VERSION env var sets global default
# Test: TOAST_DEBUGGER env var sets debugger preference
```

---

## 4. Test Framework (`lib/toast_test/`)

### 4.1 Suite System Design

```elixir
# Test: use ToastTest.Suite turns suite module into CaseTemplate (injects use ExUnit.CaseTemplate)
# Test: test modules doing `use MySuite` get __toast_suite__/0 function returning MySuite
# Test: @toast_suite attribute is set to the suite module
# Test: deployment_config/0 callback returns keyword config
# Test: mode: :auto is the default in deployment_config
# Test: setup_deployment/1 optional callback — suite without it works fine (function_exported? check)
# Test: setup_deployment/1 runs once per suite, not per test
# Test: setup_deployment/1 returning {:error, reason} → tests marked :errored, deployment stopped
# Test: setup_deployment/1 result merges into test context (override semantics — can override :client)
# Test: server_args merged with global defaults
# Test: coordinator_args applies only to coordinators
# Test: dbserver_args applies only to dbservers
# Test: agent_args applies only to agents
# Test: role-specific args take precedence over server_args for that role
```

#### Suite Discovery

```elixir
# Test: ExUnit.start(autorun: false) called before any compilation
# Test: mix toast discovers suite.ex files in suites/ subdirectories
# Test: suite modules identified by ToastTest.Suite behaviour
# Test: suite.ex compiled first (globally), then *.ex helpers (per-suite), then test_*.exs
# Test: compilation constraint: suite modules must not depend on helpers in same folder
# Test: ExUnit.Server modules_loaded/1 NOT called
# Test: orphan .exs file detection (warn on non-test_*.exs files in suite folders)
```

#### CLI

```elixir
# Test: mix toast (no args) runs all suites
# Test: mix toast smoke runs single suite
# Test: mix toast smoke shell_server runs multiple suites (space-separated)
# Test: mix toast smoke/test_version.exs runs specific file
# Test: mix toast smoke/test_version.exs:42 runs specific line
# Test: --cluster flag sets deployment mode to :cluster
# Test: --single flag sets deployment mode to :single_server
# Test: --test "pattern" filters by test name
# Test: --no-agency-dump disables agency dump
```

### 4.2 Runner Refactoring

```elixir
# Test: runner rejects modules with async: true (clear error)
# Test: runner executes tests in deterministic order (no shuffling)
# Test: runner bypasses ExUnit.Server (never calls take_sync_modules/take_async_modules)
# Test: ExUnit.Server module accumulation is harmless (dead state)
# Test: per-suite EventManager + RunnerStats created fresh
# Test: cross-suite stats accumulator aggregates total tests, failures, duration
# Test: suite abort does not affect next suite
# Test: deployment failure → all suite tests marked as :errored, proceeds to next
# Test: timeout hierarchy — global deadline > suite timeout > test timeout
# Test: default suite timeout is 1 hour
# Test: suite timeout clamped to remaining global deadline
# Test: global deadline reached mid-suite → current test aborted, remaining skipped
# Test: drain_remaining_modules uses runner's own module list, not ExUnit.Server
```

### 4.3 Crash Monitor

```elixir
# Test: CrashMonitor.handle_crash/2 calls Runner.abort!
# Test: CrashMonitor provided as :on_crash callback to Deployment.start
```

### 4.4 Test Case Template

```elixir
# Test: ToastTest.Case setup provides %{deployment: _, endpoint: _, client: _}
# Test: deployment handle read from Application.get_env(:toast, :__test_deployment__)
# Test: health check between tests rejects :degraded with clear error message naming downed servers
# Test: health check between tests rejects :failed
# Test: :ready status allows next test to proceed
```

### 4.5 Process History

```elixir
# Test: ProcessHistory records :server_started events keyed by OS PID
# Test: ProcessHistory records :server_stopped events
# Test: ProcessHistory records :server_crashed events
# Test: events timestamped
# Test: history used to correlate sanitizer log files (by PID) to server instances
# Test: history cleared between suites
```

### 4.6 Server ID Mapping

```elixir
# Test: cluster_id/2 returns cluster-internal ID for toast ID
# Test: server_by_cluster_id/2 returns server info for cluster-internal ID
# Test: mapping fetched from /_admin/cluster/health after cluster formation
# Test: mapping cached in ClusterController state
# Test: mapping stable across server restarts (data dir preserved, same ID)
# Test: control operations accept cluster_id: targeting
```

### 4.7 Interactive Test Execution

```elixir
# Test: Interactive.run/2 with module atom runs all tests in module
# Test: Interactive.run/2 with file path compiles file via Code.compile_file then runs
# Test: Interactive.run/3 with test name runs single test
# Test: recompilation replaces module in BEAM (fresh __ex_unit__ metadata)
# Test: ExUnit.Server accumulation from recompilation is harmless
# Test: results returned without deployment start/stop or result export
```

### 4.8 Inter-Suite State Cleanup

```elixir
# Test: Application.put_env keys cleaned — :__test_deployment__, :__suite_deadline__,
#       :__timeout_factor__, :__test_results__, :__test_diagnostics__,
#       :__sanitizer_matching__, :__crash_matching__
# Test: ExUnit abort table (ETS) cleared
# Test: formatter state reset (GenServers stopped and restarted)
# Test: process history observer state cleared
# Test: port allocator NOT reset (continues allocating to avoid TIME_WAIT conflicts)
```

### 4.9 Result Export

```elixir
# Test: results include suite-level grouping
# Test: global summary aggregates across suites
# Test: per-suite and per-test timing included
# Test: JUnit XML includes suite-level <testsuite> elements
```

---

## 5. REST Client (`lib/toast/client/`)

### 5.1 Core Client

```elixir
# Test: Client.new/2 creates client with base_url
# Test: Client.with_database/2 returns new client with database set
# Test: Client.with_auth/2 returns new client with auth (basic)
# Test: Client.with_auth/2 returns new client with auth (JWT via joken)
# Test: Client.with_api_version/2 with integer 1 → /_arango/v1 prefix
# Test: Client.with_api_version/2 with string "experimental" → /_arango/experimental prefix
# Test: Client.with_api_version/2 with nil → uses global default or no prefix
# Test: URL construction: version prefix → database prefix → API path
# Test: get/post/put/delete delegate to Req with correct URL and headers
# Test: auth header set correctly for basic auth
# Test: auth header set correctly for JWT
# Test: global default API version read from Toast.Config
# Test: incomplete API version warning (recommend complete version as default)
```

### 5.2 Domain Modules (Unversioned — Infrastructure)

```elixir
# Test: Collection.create/3 sends POST /_api/collection
# Test: Collection.drop/2 sends DELETE /_api/collection/{name}
# Test: Collection.list/1 sends GET /_api/collection
# Test: Document.insert/3 sends POST /_api/document/{collection}
# Test: Document.get/3 sends GET /_api/document/{collection}/{key}
# Test: Document.remove/3 sends DELETE /_api/document/{collection}/{key}
# Test: AQL.execute/2 sends POST /_api/cursor
# Test: Admin.version/1 sends GET /_api/version
# Test: unversioned modules use client's configured api_version
```

### 5.3 API Version Override (with_api_version/2)

```elixir
# Test: with_api_version/2 overrides client's api_version for subsequent domain module calls
# Test: domain module called after with_api_version(client, 1) uses /_arango/v1 prefix
# Test: with_api_version(client, nil) clears override, falls back to global default
# Test: URL construction goes through single code path regardless of version override
```

Note: Versioned domain modules (e.g., `V1.Collection`, `V1.Document`) are deferred until a real second API version exists. Tests use `with_api_version/2` instead.

---

## 6. Diagnostics (`lib/toast/diagnostics/`)

### 6.1 Coredump Analysis

```elixir
# Test: Coredump.discover/1 finds core files in server work directory
# Test: Coredump.discover/1 finds core files in /tmp filtered by PID
# Test: Coredump.discover/1 handles pipe-based core_pattern (systemd-coredump/coredumpctl)
# Test: GDB debugger constructs correct batch command
# Test: LLDB debugger constructs correct command with -c and -o flags
# Test: stack trace extraction parses frames into common struct
# Test: frame filtering removes glibc/libstdc++ internal frames
# Test: debugger auto-detection prefers LLDB, falls back to GDB
# Test: missing debugger → skip with warning, still collect core files
# Test: debugger timeout does not hang collection
# Test: binary mismatch handled gracefully (logged and skipped)
```

### 6.2 Agency Dump

```elixir
# Test: AgencyDump queries single responsive agent (not all agents)
# Test: AgencyDump fetches /_api/agency/config
# Test: AgencyDump fetches /_api/agency/state
# Test: AgencyDump fetches /_api/agency/read with [["/arango"]] body
# Test: AgencyDump skips with warning if no agents are alive
# Test: dump_agency_on_error: false disables the dump
# Test: dump_agency/1 runs before agent shutdown in stop_and_collect lifecycle
# Test: stop_and_collect uses multi-step protocol (dump_agency → shutdown → collect)
```

---

## 7. Result Packaging

```elixir
# Test: local run (no --ci) produces no packaging, prints summary and work dir path
# Test: CI run produces tier 1 files (results.json, results.xml, toast.log)
# Test: CI run produces tier 2 archive (toast-logs.tar.gz with server logs, sanitizer reports, crash reports, agency dumps)
# Test: CI run produces tier 3 files (individually compressed core dumps)
# Test: tier 3 only created when crashes exist
# Test: zstd compression used; gzip fallback if zstd unavailable
# Test: exit code 0 for all passed
# Test: exit code 1 for test failures
# Test: exit code 2 for infrastructure failure
# Test: exit code 3 for server crash
# Test: exit code 4 for sanitizer-only errors
# Test: mixed results → highest severity exit code wins (3 > 2 > 4 > 1 > 0)
```

---

## 8. Analysis Tool

```elixir
# Test: mix toast.analyze reads results.json and prints summary
# Test: --failures shows detailed failure info with stack traces
# Test: --crashes shows crash diagnostics, sanitizer errors, coredump traces
# Test: --slow N shows N slowest tests with durations
# Test: invalid file path produces clear error
# Test: malformed JSON produces clear error
```

---

## 9. Migration Plan

No TDD stubs — migration phases are verified by running existing tests after each phase:

- **Phase 1**: `mix compile` succeeds, `mix toast` runs smoke tests, `mix test` runs unit tests
- **Phase 2**: `iex -S mix` → `Toast.Deployment.start(:single_server, ...)` works; `mix xref graph` shows no ExUnit deps in `lib/toast/`
- **Phase 3**: `mix toast smoke` runs via suite system; `mix toast smoke/test_version.exs` works; `mix toast --cluster` works
- **Phase 4**: resilience test suite passes (servers stopped/killed/restarted without false alerts)
- **Phase 5**: `mix toast --ci` produces tiered packages; `mix toast.analyze results.json` works
