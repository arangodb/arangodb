# Opus Review

**Model:** claude-opus-4
**Generated:** 2026-02-22

---

## Overall Assessment

The plan is well-structured, thoughtfully phased, and demonstrates a clear understanding of the existing codebase. The architectural direction is sound: separating infrastructure from test framework, adding a suite abstraction, and building resilience testing on top. The incremental phasing is appropriate for a project without hard deadlines.

That said, there are several issues ranging from structural problems to missing considerations that should be addressed before execution begins.

---

## 1. Deployment Struct Inconsistency (Section 3.1)

The plan proposes adding a `status` field to the `Toast.Deployment` struct. But currently, the `Deployment` struct has no `status` field. Status is queried dynamically from the controller via `Deployment.status/1`. Adding `status` as a stored field creates a stale-data problem: the struct returned from `start/2` will say `:ready` forever, even if the deployment subsequently degrades or fails.

The plan needs to decide explicitly: is `status` a live query or a snapshot? If live (current approach), the struct should not include it. If snapshot, every control operation needs to return an updated struct, which is cumbersome and error-prone. The current live-query approach is better; the plan should remove `status` from the proposed struct and document that `Deployment.status/1` is the canonical way to check.

## 2. Deployment Immutability vs. Mutation (Section 3.1, 3.2)

The server control API proposes functions like `stop_server(deployment, server_id)` that operate on the `Deployment` struct. But these operations mutate state in the controller GenServer while the caller holds an immutable struct. The plan does not specify return types.

Key question: do these functions return `{:ok, updated_deployment}` or just `:ok`? The plan should clarify that control operations return `:ok | {:error, reason}` and that the `Deployment` struct is essentially a handle (containing `controller` pid) not a value object. State inspection goes through functions like `Deployment.server(deployment, id)` which query the controller live.

## 3. Race Condition in the `intentional` Flag (Section 3.2, D2)

Consider `stop_server/2`: before the controller processes the call, a crash message could arrive first. Also: what if a server crashes during intentional stop (SIGSEGV during shutdown handler)? The plan would mark this as intentional because the flag is set, but it's actually an unexpected crash. The plan should state whether crash-during-intentional-stop is treated as intentional or not, and how the signal type factors in.

## 4. HealthMonitor Suspend/Resume Is Stateless About Callers (Section 3.3)

If two operations suspend the monitor for the same server, a single resume re-enables monitoring. The plan should state this constraint. Also, adding `:suspend` needs to cancel the pending timer, otherwise a `:check` message will fire after suspension.

## 5. Suite Module Naming and Discovery Ambiguity (Section 4.1)

The discovery mechanism is underspecified. Suite folders contain `suite.ex` files, but how does the task know which module in `suite.ex` is the suite? The plan should specify: compile all `suite.ex` files, then filter loaded modules by behaviour.

## 6. Test Module to Suite Binding Is Fragile (Section 4.1)

The plan does not explain how the runner determines which suite a test module belongs to at runtime. The `use Toast.Test.Suite` macro should inject a discoverable attribute or function into test modules that the runner can query.

## 7. Sequential vs. Parallel Suite Execution (Section 4.2)

The plan never explicitly states whether suites are always sequential. The plan should state this and explain why.

## 8. Missing Gap Analysis Items Not Addressed

Items from GAP_ANALYSIS.md not covered:
- Client Tools (arangosh, arangodump, arangorestore) - Priority 1
- Failure Point Management (debugSetFailAt/debugRemoveFailAt) - Essential for resilience
- JWT Authentication - Priority 1
- SUT Checkers (post-test invariant verification) - Priority 2

## 9. `use Toast.Test.Suite` Macro Confusion (Section 4.1, 4.3)

Two different macros with similar names. The plan should clarify the migration path for existing tests and distinguish between suite definition and test module usage.

## 10. Crash Monitor Architecture (Section 3.2)

The crash monitor calls `Toast.Runner.abort!/1` directly, coupling deployment library to test framework. This must be broken in Phase 2. The plan should specify how crash notification escapes the library boundary (callbacks, PubSub, etc.).

## 11. Config Reload for Suite-Specific Configuration

Environment variables set by one suite's deployment persist for the next suite. The current `apply_toast_env/1` mutates `System.put_env`. The plan should clarify suite configuration uses explicit keyword options, not env var mutation.

## 12. `Deployment.stop_and_collect/2` Timing Issue

Coredump analysis involves running GDB (slow). The plan should specify where it runs in the lifecycle.

## 13. Exit Status Codes Are Incomplete (Section 7.2)

Missing: mixed results, sanitizer errors without test failures, timeout exit codes.

## 14. REST Client Double-Migration (Section 5)

Phase 5 (REST Client) is after Phase 3 (Suite System). Tests migrated to suites in Phase 3 use old API, then need updating in Phase 5. Consider reordering.

## 15. `check_health` Does Not Handle `:degraded`

The current `check_health/1` only considers `:ready` as healthy. `:degraded` falls through to error. This is a library-level change, not just a runner change.

## 16. Missing: Test Cleanup Between Suites

State stored in `Application.put_env`, `ExUnit.Server`, formatter state, and abort table needs clearing between suites.

## 17. Missing: Error Handling for Deployment Failures

What happens if a suite's deployment fails to start? Are tests skipped? Does the runner proceed to next suite?

## 18. Supervision Tree Gap

If a HealthMonitor crashes due to a bug, it disappears. The controller has no mechanism to detect its HealthMonitor is gone. Add process monitoring.

## 19. Coredump Discovery Security

On shared CI nodes, PID recycling could cause wrong core file collection. Handle GDB binary mismatch gracefully.

## 20. Module Naming Inconsistency

`Toast.TestCase` (current) vs `Toast.Test.Case` (plan). Use consistent naming.

---

## Summary of Recommended Changes

1. Remove `status` from the `Deployment` struct; keep as live query
2. Specify return types for control operations; clarify struct-as-handle pattern
3. Document signal-type handling for crash-during-intentional-stop
4. Add timer cancellation to HealthMonitor suspend; note single-threaded constraint
5. Specify suite discovery mechanism (behaviour scanning after compilation)
6. Define test-module-to-suite binding mechanism (module attribute injection)
7. Explicitly state suites run sequentially and why
8. Address Gap Analysis items 1.2-1.4 and 2.3, or explicitly defer with rationale
9. Clarify `Toast.TestCase` to suite CaseTemplate migration path
10. Move crash monitor's `Runner.abort!` call out of `lib/toast/` to `lib/toast_test/`
11. Address env var pollution between sequential suite deployments
12. Specify where coredump analysis runs in shutdown lifecycle
13. Define exit codes for sanitizer-only failures and mixed results
14. Consider reordering Phase 5 before/with Phase 3 to avoid double-migration
15. Add `:degraded` handling to `Deployment.check_health/1`, not just runner
16. Specify inter-suite state cleanup
17. Define behavior when a suite's deployment fails to start
18. Add HealthMonitor process monitoring from controller
