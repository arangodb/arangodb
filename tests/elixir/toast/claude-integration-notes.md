# Integration Notes — Opus Review

## Integrating

### 1. Remove `status` from Deployment struct (Review #1)
Good catch. Status is a live query, not a snapshot. The struct is a handle. Integrating.

### 2. Clarify control operation return types and struct-as-handle (Review #2)
Important clarification. Control operations return `:ok | {:error, reason}`. The Deployment struct is a handle, not a value object. All state inspection goes through query functions. Integrating.

### 3. Crash-during-intentional-stop (Review #3)
Valid edge case. The intentional flag should distinguish between expected exit and unexpected crash signals. If SIGTERM was sent and server exits normally or via SIGTERM → intentional. If SIGSEGV during shutdown → unexpected crash despite intentional stop. Integrating signal-type awareness.

### 4. HealthMonitor timer cancellation on suspend (Review #4)
Correct — pending timer must be cancelled. Single-threaded constraint on per-server control is reasonable. Integrating.

### 5. Suite discovery mechanism (Review #5)
Valid gap. Specifying: compile suite.ex, scan for behaviour implementations. Integrating.

### 6. Test-module-to-suite binding (Review #6)
Important mechanism. The `use` macro injects a `@toast_suite` module attribute. Integrating.

### 7. Sequential suite execution (Review #7)
Should be stated explicitly. Integrating.

### 8. Gap Analysis items (Review #8)
Partially integrating:
- **Client tools**: Deferring to a later phase. Not needed for initial suite system. Will note as future work.
- **Failure point management**: Adding to resilience phase — essential for resilience testing.
- **JWT authentication**: Adding to REST client phase — needed for authenticated tests.
- **SUT checkers**: Deferring to post-Phase 6. Good idea but not blocking.

### 9. Toast.TestCase migration path (Review #9)
Valid confusion. Clarifying: `Toast.TestCase` becomes the internal base that suite CaseTemplates build on. Test modules switch to `use MySuite`. Backward-compatible shim during migration. Integrating.

### 10. Crash monitor decoupling (Review #10)
Critical for the library separation goal. The deployment accepts a crash callback, `toast_test` provides its implementation. Integrating.

### 11. Env var pollution between suites (Review #11)
Good catch. Suite configuration must be explicit keyword opts, not env var mutation. Integrating.

### 12. Coredump analysis lifecycle position (Review #12)
Valid timing concern. Coredump analysis runs as a post-shutdown step with its own timeout, after the controller has stopped but before work dir cleanup. Integrating.

### 13. Exit codes for sanitizer errors (Review #13)
Important for CI. Adding exit code 4 for sanitizer-only failures. Specifying mixed results behavior. Integrating.

### 14. REST client phase ordering (Review #14)
Reasonable suggestion. Moving client refactoring to Phase 3 alongside suite system to avoid double-migration. Integrating.

### 15. check_health degraded handling (Review #15)
Correct — library-level change needed. Integrating.

### 16. Inter-suite state cleanup (Review #16)
Valid gap. Adding cleanup specification. Integrating.

### 17. Deployment failure handling (Review #17)
Important for robustness. Specifying: tests marked as errored, runner proceeds to next suite. Integrating.

### 18. HealthMonitor process monitoring (Review #18)
Good point. Controller should monitor its HealthMonitor and restart or raise alert if it dies. Integrating.

## Not Integrating

### 19. Coredump discovery security (Review #19)
Valid concern but GDB already validates the binary match. The existing handling is sufficient — GDB will report "not a core file" or "wrong binary" and we log a warning. Adding a note but not designing extra safeguards. Too minor for the plan level.

### 20. Module naming inconsistency (Review #20)
The plan uses `Toast.Test.Suite` and `Toast.Test.Case` intentionally — these are the target names after restructuring, not the current names. The current `Toast.TestCase` → `Toast.Test.Case` rename happens in Phase 1. Will add a brief note about the rename.

---

# Integration Notes — Opus Review Iteration 2

## Integrating

### R2-1. Deployment struct field migration (A1)
Valid. The plan should explicitly address what happens to `endpoint`, `work_dir`, and `crash_monitor` during migration. `endpoint` and `work_dir` are immutable after creation — adding them back to the struct avoids unnecessary GenServer round-trips. `crash_monitor` is removed in Phase 2 when callback injection replaces it. Integrating.

### R2-2. CaseTemplate layering (B1)
Important implementation detail. The suite-generated CaseTemplate delegates to `ToastTest.Case` internally. `ToastTest.Case.setup` provides base context (deployment, client, endpoint), then `setup_deployment/1` result is merged on top. The `@toast_suite` quoting must use `unquote(__MODULE__)` in the suite module's `__using__` macro. Integrating.

### R2-3. Runtime suite binding via function (B4)
Correct — `@toast_suite` is compile-time only. The macro must inject a function `def __toast_suite__, do: SuiteModule` for runtime discovery. Integrating.

### R2-4. ExUnit.Server per-suite batching (B5)
Critical design question. The plan should address this explicitly. The approach: bypass ExUnit.Server for module feeding. The runner compiles and loads test modules per-suite, then drives execution via `ExUnit.RunnerStats` and the per-test execution internals directly, without relying on ExUnit.Server's module scheduling. Integrating.

### R2-5. `expect_crash` timeout details (B3)
Valid gap. Default timeout: 30 seconds. Configurable per call via `timeout:` option. Late crash after timeout → acknowledged as edge case; the runner's `:ready` check between tests catches the failed state. Integrating.

### R2-6. Runner refactoring scope (C1)
Correct — the plan understates the work. The runner refactoring in Phase 3 is closer to "replace the outer scheduling layer while preserving per-test execution." Updating Phase 3 description. Integrating.

### R2-7. Application.put_env cleanup completeness (C2)
Valid. The cleanup list in Section 4.8 is incomplete. Enumerating all keys. Integrating.

### R2-8. ServerProcess signal-sending (C5)
Good catch. Phase 4 must include extending `ServerProcess` with `handle_call` clauses for `:kill` (SIGKILL), `:pause` (SIGSTOP), `:resume` (SIGCONT). Integrating.

### R2-9. Section numbering fix (A4)
Fixing 3.5 gap. Integrating.

### R2-10. Unify `with_api_version`/`with_api_prefix` (E1)
Good simplification. Single function accepting both integer and string: `with_api_version(client, 1)` → `/_arango/v1`, `with_api_version(client, "experimental")` → `/_arango/experimental`. Integrating.

### R2-11. Success criteria #5 fix (E2)
`--mode cluster` should be `--cluster`. Integrating.

### R2-12. `for_server` dependency direction (E3)
Valid concern. Moving to `Toast.Deployment.client(deployment, server_id)` — deployment knows about client, not vice versa. Integrating.

### R2-13. Suite compilation order constraint (B2)
Valid. Suite modules must not depend on helper modules in the same folder. Documenting. Integrating.

### R2-14. HealthMonitor suspended status (D2)
Valid. A suspended monitor is neither healthy nor unhealthy — the `healthy?/1` function should return `:suspended` or the function should be extended to a `status/1` returning `:healthy | :unhealthy | :suspended`. Integrating.

### R2-15. Coredump systemd-coredump detection (D3)
Valid. Pipe-based core patterns need `coredumpctl` integration. Adding a note. Integrating.

### R2-16. Interactive test module compilation (B6)
Valid gap. `Interactive.run/2` should accept either a module (already compiled) or a file path (will compile on the fly). Integrating.

## Not Integrating

### R2-17. Phase 3 splitting (D1)
The motivation for combining suite system and client is to avoid double-migration of tests. Splitting into 3a/3b would mean tests are migrated to suites using the old client API, then client API changes later. This is exactly what we're trying to avoid. The 14 steps are large but sequential within the phase — they can be delivered incrementally without a formal phase split. Not splitting, but adding a note about incremental delivery.

### R2-18. Integration notes namespace inconsistency (A2)
Cosmetic issue. The integration notes from iteration 1 reference the old `Toast.Test.*` naming from before the plan was updated. The plan itself is correct with `ToastTest.*`. Not updating old integration notes — they are historical.

### R2-19. `apply_toast_env` removal specificity (C3)
Already covered in the plan (Section 3.7). The plan says "refactored: environment variables are read once at task startup." The specific function name doesn't need to be in the plan. Not adding.

### R2-20. Client refactoring test count note (C4)
Valid observation but doesn't change the plan. The implementer will see the test count during migration. Not adding.

### R2-21. `check_health` vs `status` clarification (A3)
The plan already says the runner calls `status/1` (Section 4.4). The existing code calls `check_health/1`. The migration happens in Phase 3 when the runner is refactored. No additional clarification needed.

---

# Integration Notes — Opus Review Iteration 3

## Integrating

### R3-1. ExUnit.Server accumulation is harmless (Issue 1)
Critical clarification. Modules auto-register via `use ExUnit.Case` during compilation, but the runner never reads from ExUnit.Server. Dead state. `modules_loaded/1` should not be called. Integrating.

### R3-2. Suite module IS the CaseTemplate (Issue 3)
Important precision. `use ToastTest.Suite` does not generate a separate module — it turns the suite module itself into a CaseTemplate. Integrating.

### R3-3. `setup_deployment/1` runs once per suite (Issue 4)
Ambiguity resolved. `setup_deployment/1` is suite-level (once, after deployment starts). Per-test setup uses standard ExUnit `setup` blocks. Integrating.

### R3-4. Deployment handle delivery via Application.put_env (Issue 9)
Explicit statement that `Application.put_env(:toast, :__test_deployment__, deployment)` remains the mechanism. The plan eliminates env vars for *configuration*, not for the deployment *handle*. Integrating.

### R3-5. `:auto` mode resolution in runner (Issue 11)
`Toast.Deployment.start/2` only accepts `:single_server | :cluster`. The runner resolves `:auto` before calling start. Integrating.

### R3-6. Incomplete API version caveat (Issue 7)
Valid risk. Global default should be a complete version or nil. Added warning. Integrating.

### R3-7. Cross-suite stats aggregation (Issue 8)
Valid gap. Each suite gets own EventManager + RunnerStats. Runner merges stats into accumulator after each suite. Integrating.

### R3-8. `on_event` must be non-blocking (Issue 6)
Valid. Callback invoked in controller process. ProcessHistory uses GenServer.cast. Integrating.

### R3-9. `restart_server` opts specification (Issue 14)
Valid. `opts` accepts `args: [...]` for additional CLI arguments. Port/dir/binary immutable across restarts. Integrating.

### R3-10. Cluster ID mapping fetch mechanism (Issue 12)
Valid gap. Fetched from `/_admin/cluster/health`, cached in ClusterController state. Integrating.

### R3-11. Signal delivery note for SIGSTOP (Issue 13)
Valid. SIGSTOP doesn't trigger erlexec exit monitoring. Three-hop latency acceptable for low-frequency ops. Integrating.

### R3-12. `stop_and_collect` return type fix (Issue 10)
Valid. Existing code returns `diagnostics | nil`, not `{:ok, diagnostics}`. Fixing. Integrating.

### R3-13. Missing success criterion for intra-suite isolation (Issue 15)
Valid gap. Adding: "A test failure or abort within one suite does not affect subsequent suites." Integrating.

## Not Integrating

### R3-14. `test_*.exs` naming reconsideration (Issue 2)
Valid tooling concern. However, this is the user's explicit preference for visual grouping. The custom runner handles its own discovery — ExUnit's default pattern is irrelevant. Editor integration is a minor inconvenience addressed by project-level config. Keeping `test_*.exs`.

### R3-15. `:degraded` auto-recovery option (Issue 5)
Interesting idea but adds complexity. The strict abort is intentional — it forces tests to be well-behaved. Auto-recovery would mask test bugs. If this causes too much friction during development, it can be added later as an opt-in flag. Not adding now.

---

# Integration Notes — Opus Review Iteration 4

## Integrating

### R4-1. Agency dump lifecycle ordering (Issue 1)
Critical blocking detail. `stop_and_collect/1` now uses a multi-step protocol: first `dump_agency/1` GenServer call, then `shutdown/2`, then log/sanitizer collection, then coredump analysis. Integrating.

### R4-2. ExUnit.start timing (Issue 8)
Critical sequencing detail. `mix toast` must call `ExUnit.start(autorun: false)` before any suite compilation. Integrating.

### R4-3. `setup_deployment` error handling (Issue 12)
Unspecified behavior. On error: tests marked as `:errored`, deployment stopped, runner proceeds. Integrating.

### R4-4. `on_event` from cluster tasks (Issue 3)
Valid constraint violation. ClusterController tasks send events back to controller via `GenServer.cast`, controller forwards to `on_event` in `handle_cast`. Integrating.

### R4-5. Port allocator should NOT reset (Issue 7)
Valid TCP TIME_WAIT concern. Reversed recommendation: continue allocating, don't reset. Integrating.

### R4-6. Suite timeout hierarchy (Issue 11)
Valid gap. Added: global deadline > suite timeout (default 1h) > test timeout. Integrating.

### R4-7. `server_args` role-specific support (Issue 13)
Valid. Added `coordinator_args`, `dbserver_args`, `agent_args` alongside `server_args`. Integrating.

### R4-8. `setup_deployment` context merge override semantics (Issue 5)
Valid. Documented as explicit override semantics. Integrating.

### R4-9. Optional callback dispatch (Issue 15)
Valid. `function_exported?/3` for runtime dispatch. Added note. Integrating.

## Not Integrating

### R4-10. `check_health` structured returns (Issue 2)
Implementation detail the developer will handle during Phase 3. The plan says the runner uses `status/1` and provides differentiated error messages — the developer will implement this naturally. Not adding plan-level detail.

### R4-11. Versioned module URL construction single code path (Issue 4)
Valid implementation concern but obvious to any competent Elixir developer. URL prefix logic lives in the core `Toast.Client` request path; versioned modules call `with_api_version` before delegating. Not adding plan-level detail.

### R4-12. `Code.compile_file` vs `Code.require_file` (Issue 6)
Minor. The interactive module handles this internally. Not adding plan-level detail.

### R4-13. Cluster ID mapping stability across restarts (Issue 9)
Correct observation but doesn't change the plan. ArangoDB persists server IDs. Mapping stays valid. Not adding.

### R4-14. `drain_remaining_modules` rewrite (Issue 10)
Implicit in the ExUnit.Server bypass design. The developer will rewrite this when replacing the scheduling layer. Not adding separate note.

### R4-15. `verify_crash` return when expectation expired (Issue 14)
Edge case that the developer will handle. The existing `{:error, :timeout}` return suffices — the server crashed after the timeout, so from `verify_crash`'s perspective, the expectation timed out. Not adding.

---

# Integration Notes — External Review Iteration 5

## Integrating

### R5-1. ExUnit.Server bypass — ToastTest.ExUnitCompat adapter (C1)
Critical maintainability improvement. The runner depends on undocumented ExUnit internals: `ExUnit.RunnerStats`, `ExUnit.EventManager`, `module.__ex_unit__()` (whose struct shape changed in Elixir 1.17). Wrapping these in a `ToastTest.ExUnitCompat` adapter isolates version-specific assumptions. Add a compile-time check pinning to a specific Elixir version range. This is the single highest-leverage action for maintainability. Integrating into the runner (Section 4.2 / section-05).

### R5-2. Application.put_env → ETS-based registry for deployment delivery (C2)
Valid concurrency hazard. ExUnit's `on_exit` callbacks run in spawned processes. If a lingering `on_exit` from test N reads `:__test_deployment__` after the next suite's deployment is stored, it sees the wrong deployment. Replace `Application.put_env(:toast, :__test_deployment__, deployment)` with an ETS-based registry keyed by suite module. `ToastTest.Case.setup` reads from the registry using the test's `@toast_suite` attribute to key the lookup. Eliminates the global mutable channel. Integrating into Section 4.4 (Case Template), Section 4.8 (State Cleanup), and section-05 (Runner).

### R5-3. Module namespace requirement for suite-local modules (C4)
Valid concern. If two suites define `Helpers` as a top-level module, the second compilation silently replaces the first in the BEAM. Suite-local helper modules (`.ex` files in suite folders) MUST be namespaced under the suite: `Smoke.Helpers`, `ShellServer.CrudHelpers`, not bare `Helpers`. The suite discovery step adds a compiler check: after compiling a suite's helpers, verify all defined modules are namespaced under the suite's top-level namespace. Emit a warning for violations. Integrating into Section 4.1 (Suite Discovery).

### R5-4. on_event — direct callback from spawned tasks (C5)
Valid simplification. The current plan routes events through the ClusterController mailbox (tasks → GenServer.cast → controller → on_event callback), adding latency and potential reordering if a crash arrives before the start event is processed. Since the `on_event` callback is already specified as non-blocking, spawned tasks can call it directly. The `ProcessHistory` GenServer handles ordering via timestamps. This simplifies the event path and eliminates the controller bottleneck. Updating the plan to state that tasks call `on_event` directly. The controller still calls `on_event` for events it generates itself (unexpected crashes detected via erlexec `:DOWN` messages). Integrating into Section 3.2 (Controller Architecture) and Section 4.5 (Process History).

### R5-5. Defer versioned domain modules — YAGNI (C6)
Agreed. Today there is exactly one API version. The versioned module scheme (`Toast.Client.V1.Collection`, etc.) presupposes multiple incompatible API versions. Since new versions are rare by design, this is premature. Start with unversioned modules + `with_api_version/2` for explicit versioning at call sites. Add versioned modules only when a second API version actually exists with different function signatures. This removes the `lib/toast/client/v1/` directory and related tests. The `with_api_version/2` function already handles explicit version pinning for test-target operations. Integrating into Section 5 (REST Client).

### R5-6. mix test / mix toast coexistence — test_helper.exs (C9)
Valid gap. After flattening the umbrella, `mix test` will find `test/test_helper.exs`. Currently, the smoke test's `test_helper.exs` calls `Toast.TestCase.setup_suite()` which starts a deployment. Unit tests must NOT start a server. Phase 1 should explicitly state that `test/test_helper.exs` contains only `ExUnit.start(exclude: [:integration])` (which is actually already the case for `apps/toast/test/test_helper.exs`). Adding an explicit note in Section 1 (Restructure) to preserve this and document the invariant. Integrating.

### R5-7. .toast.local.exs — skip in CI mode (C12)
Valid safety measure. When `--ci` or `TOAST_CI=true` is set, skip evaluation of `.toast.local.exs`. This prevents developer-specific configuration from affecting CI runs and eliminates the security concern of evaluating arbitrary Elixir code in CI. Integrating into Section 3.6 (Configuration).

### R5-8. TOAST_COREDUMP_DIR env var override (C10)
Valid addition. Non-systemd core handlers (e.g., apport on Ubuntu) may store core files in non-standard locations. Adding a `TOAST_COREDUMP_DIR` env var that overrides the discovery search path. When set, the coredump discovery function searches only that directory (in addition to the server work directory). Integrating into Section 6.1 (Coredump Discovery).

### R5-9. Suite compilation failure handling (Gap)
Valid gap. A compile error in one `suite.ex` should not prevent other suites from running. The mix task should wrap per-suite compilation in `try/rescue`, catch `CompileError` and `Code.LoadError`, mark that suite as errored (all tests get `:errored` status with the compile error message), and proceed to the next suite. Integrating into Section 4.1 (Suite Discovery).

### R5-10. ExUnit.after_suite callback accumulation (Gap)
Valid concrete bug. `ExUnit.after_suite/1` registers callbacks that accumulate globally and are never cleared. In multi-suite execution, callbacks from suite 1 would fire during suite 2's teardown. The inter-suite cleanup (Section 4.8) must handle this. The specific fix: after each suite completes, clear the accumulated `after_suite` callbacks from ExUnit's internal state. This requires accessing ExUnit internals (another argument for the ExUnitCompat adapter from R5-1). Integrating into Section 4.8 (State Cleanup).

### R5-11. Test data isolation — acknowledge gap (Gap)
Valid observation. The plan enforces `:ready` deployment state between tests but says nothing about data cleanup. If test A creates a collection, test B sees it. This is a deliberate design choice: data isolation is the test's responsibility, not the framework's. Adding an explicit acknowledgment in the plan with guidance: tests should clean up their own data (create/drop collections, databases), or suites can use `setup_deployment/1` to create a test database and `teardown_deployment/1` to drop it. Full database sandboxing (like Ecto Sandbox) is not feasible for ArangoDB without a transaction-based approach. Adding to Section 10 (Design Decisions) as D11.

### R5-12. Partial deployment diagnostics (Gap)
Valid edge case. If `setup_deployment/1` fails, the deployment is partially started — some servers may be up, others not. `stop_and_collect/1` must handle this gracefully: attempt to collect diagnostics from whatever is available, don't crash if some servers are already down. The existing `stop_and_collect/1` already handles controller errors by returning `nil` diagnostics, but the note should be explicit. Integrating into Section 4.2 (Runner) as a note in the deployment failure handling section.

### R5-13. Phase 3 incremental delivery note (C8)
The plan already notes that the REST client can land first within Phase 3 (no dependency on suite system). Strengthening this note to explicitly recommend shipping the client before the suite system, since the client is independent and low-risk while the suite system (ExUnit.Server bypass) is the riskiest part of the entire plan. Not splitting into formal sub-phases (already declined in R2-17 for double-migration reasons). Integrating as a stronger note in Phase 3.

### R5-14. Log size limits — acknowledge gap (Gap)
Valid practical concern. Long-running suites can produce multi-GB server logs. Tier 2 compression of very large logs may time out or OOM. Adding a note in Section 7 (Result Packaging) that the tier 2 archiving should stream files into the tar archive rather than loading them into memory, and that a configurable log size cap (e.g., tail last N MB) could be added as a future enhancement. Not designing the full solution now.

## Not Integrating

### R5-15. Suite module triple duty — macro complexity (C3)
The three levels of `__using__` macro nesting are an inherent cost of the ExUnit CaseTemplate pattern. Separating config from CaseTemplate would add a second module per suite, increasing the surface area (more files, more imports, more confusion about which module to `use`). The current design is deliberate: the suite module IS the CaseTemplate. This was explicitly designed across three iterations of review. A `mix toast.debug` command for inspecting macro expansion is implementation-level detail, not plan-level. The macro expansion is well-specified in R2-2 and R3-2. Not changing.

### R5-16. test_ prefix → *_test.exs suffix (C7)
Already explicitly decided in R3-14 with user agreement. The custom runner handles its own file discovery — ExUnit's default `*_test.exs` pattern is irrelevant for suites. The `test_` prefix provides visual grouping in directory listings (test files sort together, separate from `suite.ex` and helpers). IDE integration is addressable via project-level config. The tooling concern is valid but outweighed by the discovery simplicity and directory ergonomics the user prefers. Not changing.

### R5-17. HealthMonitor stale :check messages (C11)
Already handled. Section 02 (Step 6) and Section 06 both specify adding a `:suspended` clause to the `:check` handler: `def handle_info(:check, %{status: :suspended} = state), do: {:noreply, state}`. No additional change needed. The reviewer may not have seen this detail in the sections.

---

# Integration Notes — External Review Iteration 6

## Integrating

### R6-1. Split Phase 3 into 3a (REST Client) and 3b (Suite System) (B1)
The reviewer is right — Phase 3 bundles two independent workstreams. The "avoid double-migration" rationale from R2-17 is weak given the smoke tests are ~3 trivial files. Splitting gives independent deliverables and lets the riskier runner rewrite proceed without blocking client delivery. The REST client (steps 7-10) becomes Phase 3a; the suite system + runner rewrite + migration (steps 1-6, 11-14) becomes Phase 3b. This supersedes the softer "ship the client first" note from R5-13. The plan note about combining to avoid double-migration is removed.

### R6-2. Runner prototype gate with success criteria and fallback (B2)
Excellent risk management. The runner rewrite is the highest-risk item in the entire plan. Adding explicit success criteria for a prototype gate before committing to the full rewrite:
- **Prototype criteria**: Can compile one suite, start a deployment, run 5 tests, abort on crash, produce stats.
- **Bounded timebox**: Define upfront (e.g., 1 week).
- **Fallback plan**: If the prototype reveals the ExUnit.Server bypass is too fragile, fall back to a thin wrapper over ExUnit.Server that resets state between suites (less clean but lower risk).

### R6-3. Enumerate ALL ExUnit internal dependencies before Phase 3b (B3)
The ExUnitCompat adapter was introduced in R5-1 but needs full specification. Adding an explicit enumeration task as the first step of Phase 3b. Known dependencies to enumerate:
- `ExUnit.EventManager.start_link/0`, `.suite_started/2`, `.suite_finished/2`, `.module_started/2`, `.module_finished/2`, `.test_started/2`, `.test_finished/2`
- `ExUnit.RunnerStats.init/1`, `.stats/1`
- `module.__ex_unit__()` — returns `%ExUnit.TestModule{}` struct
- `%ExUnit.Test{}` struct fields (`:state`, `:tags`, `:time`)
- `ExUnit.Server.add_module/2` (harmless auto-registration, but must understand)
- `ExUnit.OnExitHandler` — handles `on_exit` callbacks
- `ExUnit.CaseTemplate.__using__/2` macro internals
- `ExUnit.Callbacks` — setup/setup_all lifecycle
- `Application.get_env(:ex_unit, ...)` — ExUnit configuration keys
- Process dictionary keys set by ExUnit during test execution
This list becomes the ExUnitCompat adapter's specification. Every item must be wrapped.

### R6-4. Replace Application.put_env with %ToastTest.SuiteRun{} context struct (B4)
Strong design improvement. Currently 6 Application.put_env keys serve as global mutable cross-component state with explicit inter-suite cleanup. Replace with a `%ToastTest.SuiteRun{}` struct threaded through the runner:

```elixir
%ToastTest.SuiteRun{
  suite_module: module(),
  deployment: Toast.Deployment.t() | nil,
  suite_deadline: integer(),
  timeout_factor: float(),
  results: [map()],
  diagnostics: map(),
  sanitizer_matching: map(),
  crash_matching: map()
}
```

The runner creates one per suite, threads it through execution functions, and lets it go out of scope when done. No cleanup needed — the struct is local, not global. For values needed deep in the execution stack (deadline, timeout_factor), pass through the runner's existing `config` parameter to `run_module/5`. Deployment delivery stays in ETS registry (read by test setup callbacks). This eliminates the entire 6-key Application.put_env usage and simplifies the cleanup to just: ETS deployment registry, abort table, after_suite callbacks, formatters, process history.

### R6-5. Defer ProcessHistory to Phase 4 (B5)
ProcessHistory is only needed for resilience tests with server restarts (correlating diagnostics to specific server instances across restarts). The existing CrashMatcher/SanitizerMatcher already use timestamps for basic correlation, which is sufficient for Phase 3b. Move ProcessHistory from Phase 3b to Phase 4 (Resilience). The `:on_event` callback mechanism stays in Phase 2 (callback injection), but the ProcessHistory consumer is deferred.

### R6-6. Simplify expect_crash/verify_crash — drop the ref (B6)
At most one pending expectation per server (concurrent ops serialized by GenServer mailbox). Server ID is sufficient for identification. The ref adds indirection without value. Change:
- `expect_crash(deployment, server_id, opts)` → `:ok` (was `{:ok, ref}`)
- `verify_crash(deployment, server_id, opts)` → `{:ok, crash_info} | {:error, :not_crashed} | {:error, :timeout}` (was `verify_crash(deployment, ref, opts)`)

### R6-7. Address BEAM code loading between suites (B7)
When test modules are compiled for suite A and suite B sequentially, suite A's modules stay in memory. Over many suites, this accumulates. Add a note about `:code.purge/1` for completed suites' test modules as an optional cleanup step. Not making it mandatory (memory impact is likely small for test modules), but documenting the consideration and providing the mechanism.

### R6-8. Namespace enforcement for test modules (not just helpers) (M8)
Extend the existing namespace check (R5-3) from helpers only to include test modules. Two suites could both define `VersionTest` at the top level. The BEAM would silently replace the first. Check that all compiled test modules are namespaced under the suite's root namespace.

### R6-9. Document dead handle pattern (M9)
The deployment struct holds a controller PID. If the controller dies (deployment stops), calling `status/1` or `servers/1` will fail with `:noproc`. Add a note to the Deployment API documenting this behavior: "After `stop/1` or `stop_and_collect/1`, the deployment handle is invalid. Calling query functions on a stopped deployment returns `{:error, :stopped}`."

### R6-10. Configurable health check between tests — per-suite callback (M10 + user comment)
The reviewer suggests making the health check opt-in. The user expands: "suites can register custom logic." Add `between_tests` as an optional callback on `ToastTest.Suite`:

```elixir
@callback between_tests(Toast.Deployment.t(), ExUnit.Test.t()) :: :ok | {:error, term()}
@optional_callbacks [between_tests: 2]
```

Suites configure via `use ToastTest.Suite`:
- Default (no option): check deployment status is `:ready` (current behavior)
- `between_tests: false`: disable the check entirely (fast suites with no server manipulation)
- `between_tests: &MySuite.custom_check/2`: custom callback for suites with specific needs

This is a clean extension of the behaviour pattern.

## Not Integrating

### R6-11. User comment on API versioning
The user confirms the deferral of versioned modules (D9/R5-5) is correct and notes a new API version is planned. No plan change — we add versioned modules when implementing the new version. Acknowledged.

### R6-12. Phase 3 "avoid double migration" rationale
This rationale is being removed (superseded by R6-1). The phase split makes double-migration moot — the smoke tests are trivial to migrate in Phase 3a (minimal, just update client calls) and again in Phase 3b (full suite structure).
