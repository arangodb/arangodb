<!-- PROJECT_CONFIG
runtime: elixir-mix
test_command: mix test
END_PROJECT_CONFIG -->

<!-- SECTION_MANIFEST
section-01-restructure
section-02-library-extraction
section-03-rest-client
section-04-suite-system
section-05-runner
section-06-server-control
section-07-resilience
section-08-diagnostics
section-09-ci-packaging
END_MANIFEST -->

# Implementation Sections Index

## Dependency Graph

| Section | Depends On | Blocks | Parallelizable |
|---------|------------|--------|----------------|
| section-01-restructure | - | all | No |
| section-02-library-extraction | 01 | 03, 04, 05, 06, 07, 08, 09 | No |
| section-03-rest-client | 02 | 07, 09 | Yes |
| section-04-suite-system | 02 | 05 | Yes |
| section-05-runner | 04 | 07, 09 | No |
| section-06-server-control | 02 | 07 | Yes |
| section-07-resilience | 03, 05, 06 | - | No |
| section-08-diagnostics | 02 | 09 | Yes |
| section-09-ci-packaging | 05, 08 | - | No |

## Execution Order

1. section-01-restructure (no dependencies)
2. section-02-library-extraction (after 01)
3. section-03-rest-client, section-04-suite-system, section-06-server-control, section-08-diagnostics (parallel after 02)
4. section-05-runner (after 04)
5. section-07-resilience (after 03, 05, 06)
6. section-09-ci-packaging (after 05, 08)

## Section Summaries

### section-01-restructure
Phase 1: Flatten umbrella to single Mix project. Move files from `apps/toast/` and `apps/smoke_test/` to `lib/`, `test/`, `suites/` structure. Rename modules (`Toast.TestCase` -> `ToastTest.Case`, `Toast.Runner` -> `ToastTest.Runner`). Update `mix.exs`. Verify `mix toast` and `mix test` both work. Delete `apps/` directory.

Plan sections: 2 (Project Structure), 9 Phase 1.

### section-02-library-extraction
Phase 2: Separate `lib/toast/` (infrastructure library) from `lib/toast_test/` (ExUnit integration). Refactor Deployment struct to handle pattern (immutable `endpoint`, `work_dir`, controller PID; mutable state via live queries). Implement controller architecture with server state tracking (`intentional` flag, operational states). Implement HealthMonitor with `:healthy`/`:unhealthy`/`:suspended` status. Set up supervision tree (PortAllocator, Process.Supervisor, Deployment.Supervisor). Add `:on_crash` and `:on_event` callback injection to break library-test coupling. Refactor config loading (env vars read once, `.toast.local.exs` support). Verify `lib/toast/` has zero ExUnit dependencies. Verify IEx workflow.

Plan sections: 3.1 (Deployment API — struct and core operations only, not control ops), 3.2 (Controller Architecture — state tracking and crash notification), 3.3 (Health Monitor Updates), 3.4 (Application Supervision Tree), 3.6 (Configuration), 9 Phase 2.

### section-03-rest-client
Phase 3 (client portion): Refactor `Toast.Client` into core HTTP module + unversioned domain modules (Collection, Document, AQL, Index, Admin) + versioned domain modules (V1.Collection, V1.Document). Implement `with_database/2`, `with_auth/2`, `with_api_version/2` scoping functions. Add JWT authentication (`joken` dependency). Implement URL-path-based API versioning (`/_arango/vX/` prefix construction) with globally configurable default version. Add `Toast.Deployment.client/2` for server-specific clients (by Toast ID, role, or cluster-internal ID).

Plan sections: 5 (REST Client).

### section-04-suite-system
Phase 3 (suite portion): Define `ToastTest.Suite` behaviour (`deployment_config/0`, optional `setup_deployment/1`, `teardown_deployment/1`). Implement `use ToastTest.Suite` macro that turns the suite module itself into a CaseTemplate, injecting `@toast_suite` attribute and `__toast_suite__/0` function. Implement `ToastTest.Case` base template providing `%{deployment, client, endpoint}` context. Implement suite discovery in `mix toast` task (compile `suite.ex`, then `*.ex` helpers, then `test_*.exs` — with `ExUnit.start(autorun: false)` before compilation). Implement `ToastTest.ProcessHistory` observer for process lifecycle events. Implement server ID mapping (`cluster_id/2`, `server_by_cluster_id/2`). Implement `ToastTest.Interactive.run/2` for IEx-based test execution. Define inter-suite state cleanup (all 7 Application.put_env keys, ETS abort table, formatter state, process history, port allocator NOT reset). Implement path-based CLI (`mix toast smoke`, `mix toast smoke/test_version.exs:42`, `--cluster`/`--single`, `--test`). Add orphan file detection. Implement result export with suite-level grouping.

Plan sections: 4.1 (Suite System Design), 4.4 (Test Case Template), 4.5 (Process History), 4.6 (Server ID Mapping), 4.7 (Interactive Test Execution), 4.8 (Inter-Suite State Cleanup), 4.9 (Result Export).

### section-05-runner
Phase 3 (runner portion): Rewrite `ToastTest.Runner` outer scheduling layer. Remove async scheduling logic, remove test module shuffling/seed support, enforce synchronous-only execution. Bypass ExUnit.Server for module feeding (accept that modules auto-register as harmless dead state; drive execution via `module.__ex_unit__()` directly). Implement per-suite execution with deployment lifecycle (start deployment, run tests, collect diagnostics, stop). Implement suite-level timeouts clamped to global deadline (hierarchy: global > suite 1h default > test). Implement suite abort on crash (only that suite's remaining tests skipped). Implement deployment failure handling (tests marked `:errored`, proceed to next suite). Implement `:degraded` rejection between tests. Extract `ToastTest.CrashMonitor` from deployment module (`:on_crash` callback that calls `Runner.abort!`). Implement cross-suite stats aggregation (per-suite EventManager + RunnerStats, merged into accumulator). Migrate smoke tests to new suite structure.

Plan sections: 4.2 (Runner Refactoring), 4.3 (Crash Monitor), 9 Phase 3 steps 3-6 and 11-14.

### section-06-server-control
Phase 4 (control operations): Extend `ServerProcess` GenServer with `handle_call` for `:kill` (SIGKILL), `:pause` (SIGSTOP), `:resume` (SIGCONT) via `:exec.kill/2`. Note SIGSTOP does not trigger erlexec exit monitoring. Add signal-type awareness to controllers (SIGSEGV during intentional stop clears intentional flag). Implement control operations on Deployment: `stop_server`, `kill_server`, `pause_server`, `resume_server`, `restart_server`, `start_server` — with server ID, role-based (`role: :dbserver`), and cluster-internal-ID targeting. Implement `:suspend`/`:resume` messages on HealthMonitor with timer cancellation. Add `:degraded` deployment status to controllers. Add HealthMonitor process monitoring from controllers (restart on unexpected death). Implement on SingleServerController and ClusterController.

Plan sections: 3.1 (Server Control Operations), 3.2 (Controller changes for control ops), 3.3 (Health Monitor suspend/resume), 9 Phase 4 steps 1-3, 5-7.

### section-07-resilience
Phase 4 (resilience testing): Implement `expect_crash`/`verify_crash` for failure-point-triggered crashes (default 30s timeout, configurable, auto-clearing). Implement failure point management (`set_failure_point`, `clear_failure_point`, `clear_all_failure_points`) with role/server targeting via ArangoDB debug API. Implement cluster-internal server ID mapping (fetch from `/_admin/cluster/health`, cache in ClusterController state). Write proof-of-concept resilience test suite demonstrating: stop/restart servers, pause/resume, kill and recover, expect_crash with failure points, verify health monitoring behaves correctly during deliberate actions.

Plan sections: 3.1 (expect_crash/verify_crash), 3.5 (Failure Point Management), 4.6 (Server ID Mapping), 9 Phase 4 steps 4, 8-10.

### section-08-diagnostics
Phase 5 (diagnostics): Implement `Toast.Diagnostics.Coredump` with pluggable debugger backend (`Debugger` behaviour with GDB and LLDB implementations). Core file discovery (work dir, `/tmp/core*` by PID, `core_pattern`, `coredumpctl` for systemd-coredump). Stack trace extraction (batch-mode debugger execution, thread/frame parsing, crash location). Auto-detection and configuration via `.toast.local.exs`/`TOAST_DEBUGGER`. Implement `Toast.Diagnostics.AgencyDump` for cluster diagnostics (query single responsive agent for `/_api/agency/{config,state,read}`). Integrate into `stop_and_collect/1` lifecycle: agency dump (pre-shutdown GenServer call) -> shutdown -> log/sanitizer collection -> coredump analysis (post-shutdown with own timeout).

Plan sections: 6.1 (Debugger Integration), 6.2 (Agency Dump), 9 Phase 5 steps 1-4.

### section-09-ci-packaging
Phase 5 (packaging + analysis): Implement tiered result packaging — Tier 1 (always published: results.json, results.xml, toast.log), Tier 2 (compressed archive: server logs, sanitizer reports, crash reports, agency dumps), Tier 3 (individually compressed: core dumps, database dirs). Implement `--ci` flag/`TOAST_CI` env var for local vs CI behavior. Define exit codes 0-4 with severity ordering (highest wins for mixed results). Implement `mix toast.analyze` task with analysis modules: Summary (pass/fail, durations, suite breakdown), Failures (messages, stack traces, related diagnostics), Crashes (crash reports, sanitizer errors, coredump traces), Performance (slowest tests, duration distribution). CircleCI integration configuration.

Plan sections: 7 (Result Packaging), 8 (Analysis Tool), 9 Phase 5 steps 5-8.
