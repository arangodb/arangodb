# Toast Gap Analysis

Comparison of the Elixir Toast implementation against the Python Armadillo framework
and the legacy JS framework (analyzed in `tests/python/plan/`).

Items are grouped by priority. Each item tracks what exists in the reference
implementations, what Toast currently has, and what needs to happen.

## Legend

- **[DONE]** — Implemented and working
- **[OPEN]** — Not yet started
- **[PARTIAL]** — Some functionality exists, needs completion

---

## Priority 1 — Core Gaps (blocking broader adoption)

### 1.1 [OPEN] Client: Database Selection

**Reference**: Both JS and Python allow targeting a specific database (`/_db/{name}/_api/...`).

**Toast**: All `Client` operations implicitly target `_system`. No way to select a
different database.

**Action**: Add `database` option to `Client.new/2` that prefixes the base URL.

---

### 1.2 [OPEN] Client Tools (arangosh, arangodump, arangorestore, ...)

**Reference**: JS has comprehensive wrappers for arangosh (interactive + script execution),
arangodump, arangorestore, arangoimport, arangoexport, arangobench, arangobackup.
Each has its own config builder with parameter validation. Python has a generic
`ProcessExecutor` for one-shot commands.

**Toast**: Not implemented. Tests can only interact via the REST API.

**Action**: Start with `Toast.ClientTools.Arangosh` for script execution (most commonly
needed), then add dump/restore. Use erlexec (already a dependency) for process management.

---

### 1.3 [OPEN] Failure Point Management

**Reference**: JS provides `debugSetFailAt` / `debugRemoveFailAt` via REST API.
The SUT checker verifies no active failure points remain after tests.

**Toast**: Not implemented.

**Action**: Add `Client.set_failure_point/3` and `Client.clear_failure_points/1`.

---

### 1.4 [OPEN] JWT Authentication

**Reference**: Both JS and Python generate JWT HS256 tokens for authenticating
against arangod when `server.authentication` is enabled. Includes secret management,
token rotation, and auth header generation.

**Toast**: Absent. No auth module, no `joken` dependency, no auth headers on Client.
Tests only work with `server.authentication=false`.

**Action**: Add `Toast.Utils.Auth` with JWT HS256 via `joken`. Wire into Client as
an option.

---

## Priority 2 — Important for CI / Production Use

### 2.1 [OPEN] Global Execution Deadline

**Reference**: Python has `TimeoutManager` with layered scopes (global -> test ->
operation), watchdog enforcement, and `DeadlineExceededError`. JS has global deadline
and output-idle timeouts.

**Toast**: Individual operations have timeouts (startup, shutdown, health check).
`ClusterController` tracks deadlines internally during deploy. But there is no
session-level deadline that aborts the entire suite if exceeded.

**Action**: Add `TOAST_TIMEOUT` env var for global deadline. Implement as a watchdog
process that aborts ExUnit when the deadline is exceeded.

---

### 2.2 [OPEN] Sanitizer-to-Test Matching

**Reference**: Python's `sanitizer_matcher.py` correlates sanitizer log files to
specific tests by timestamp. High/low confidence levels with configurable tolerance
(default 5s for async write delay).

**Toast**: Sanitizer errors are collected per-server with file timestamps, but never
correlated to specific tests. Developers don't know which test triggered an error.

**Action**: Record test start/end timestamps in `ResultFormatter`. After suite
completion, match sanitizer file mtimes to test execution windows.

---

### 2.3 [OPEN] SUT Checkers (Post-Test Invariant Verification)

**Reference**: JS has 9 checker domains: collections, databases, graphs, views, users,
tasks, transactions, failure points, analyzers. Each captures a baseline snapshot
before the test, then diffs after. Configurable severity policies and remediation hooks.

**Toast**: Not implemented. Tests that leak resources silently corrupt subsequent tests.

**Action**: Start with the most impactful checkers — collections, databases, and
transactions. Implement as an `on_exit` callback in `Toast.TestCase`.

---

### 2.4 [OPEN] Server Restart

**Reference**: Both JS and Python support restarting individual servers. Required for
resilience testing (kill and restart, leader failover scenarios).

**Toast**: `ServerProcess` has no restart capability. Once stopped, cannot be restarted.

**Action**: Add `ServerProcess.restart/2` that stops and relaunches. Wire through
`Controller` / `ClusterController`.

---

## Priority 3 — Nice to Have

### 3.1 [OPEN] GDB / Core Dump Integration

**Reference**: JS has automated core dump detection (systemd-coredump, /var/tmp
patterns), GDB integration with automated script execution, stack trace extraction,
and core file pattern handling. Stack filtering to reduce noise.

**Toast**: `CrashLogParser` only parses arangod's own crash log output. No core dump
discovery or GDB integration.

**Action**: Add `Toast.Diagnostics.CoreDump` for core file discovery and optional GDB
stack trace extraction.

---

### 3.2 [OPEN] Per-Test Timeouts

**Reference**: Python supports per-test timeout enforcement via the timeout manager.
JS has per-test and per-suite timeout options.

**Toast**: No per-test timeout enforcement. Tests can hang indefinitely (unless a
global deadline is added per 2.1).

**Action**: Add timeout option to `Toast.TestCase` setup that wraps each test in a
`Task.async` with a deadline.

---

### 3.3 [OPEN] Configuration File Support

**Reference**: Python has three-level config precedence (CLI > env > YAML file > defaults).

**Toast**: Two levels (keyword opts > env vars > defaults). No config file support.

**Action**: Low priority. Env vars are sufficient for CI. Consider adding YAML/TOML
config file support if there's demand.

---

## Completed (from implementation plan phases 0-6)

- [DONE] Umbrella project structure
- [DONE] Process management via erlexec (ServerProcess)
- [DONE] Port allocation (PortAllocator)
- [DONE] Single server deployment (Controller)
- [DONE] Cluster deployment (ClusterController)
- [DONE] Command building (CommandBuilder)
- [DONE] Health checks + agency consensus (Health)
- [DONE] Build/binary detection (Filesystem)
- [DONE] Sanitizer env var handling + log collection (Sanitizer)
- [DONE] Crash log parsing (CrashLogParser)
- [DONE] Server log scanning (ServerLog)
- [DONE] ExUnit integration (TestCase)
- [DONE] REST client (Client)
- [DONE] CLI formatter (CLIFormatter)
- [DONE] Result collection (ResultFormatter)
- [DONE] JSON export (ResultExporter.JSON)
- [DONE] JUnit XML export (ResultExporter.JUnitXML)
- [DONE] Suite generator (mix toast.gen.suite)
- [DONE] Crash detection + test abort
- [DONE] Smoke test suite
