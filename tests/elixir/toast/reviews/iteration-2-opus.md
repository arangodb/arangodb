# Opus Review — Iteration 2

**Model:** claude-opus-4-6
**Generated:** 2026-02-23

---

## Overall Assessment

The plan has materially improved since the first review. The Deployment struct is now a clean handle, control operation return types are specified, signal-type awareness is documented, sequential execution is explicit, and the library-framework boundary via callback injection is well-articulated. The plan reads as implementable by someone who knows the codebase.

The remaining issues fall into three categories: (A) internal inconsistencies within the plan itself, (B) underspecified mechanisms that will force ad-hoc design decisions during implementation, and (C) friction between what the plan proposes and what the existing code actually does.

---

## A. Internal Inconsistencies

### A1. Deployment Struct Still Contains `endpoint` and `servers` in Existing Code, Plan Removes Them but Does Not Address Migration

The plan (Section 3.1) defines the Deployment struct as:

```elixir
%Toast.Deployment{
  id: String.t(),
  mode: :single_server | :cluster,
  config: Toast.Config.t(),
  controller: pid()
}
```

The existing struct has `endpoint`, `servers`, `crash_monitor`, and `work_dir`. The plan proposes replacing `endpoint` and `servers` with live queries, which is architecturally sound. But:

- `endpoint` is populated at startup and never changes. The plan should note whether `endpoint/1` is actually dynamic or is a startup-time constant exposed through a query for consistency. If constant, there is an argument for keeping it on the struct.
- `work_dir` is used in the `after_suite` callback for directory cleanup. The proposed struct drops it. The plan should specify where `work_dir` lives.
- `crash_monitor` is managed by `Toast.Deployment.start/2`. The plan moves crash monitoring to callback injection. The plan should explicitly state that `crash_monitor` field and `spawn_crash_monitor/0` / `stop_crash_monitor/1` are removed during Phase 2.

### A2. Module Namespace Inconsistency Between Integration Notes and Plan

The integration notes (item 20) state: "The plan uses `Toast.Test.Suite` and `Toast.Test.Case` intentionally." But the plan itself uses `ToastTest.Suite` and `ToastTest.Case` throughout. The integration notes appear to reference a naming scheme the plan no longer uses.

### A3. `check_health` Handling of `:degraded` — Plan vs. Existing Code

The plan (Section 4.4) says the runner checks `Toast.Deployment.status/1` between tests. But the existing runner calls `check_health/1`, which already returns `{:error, ...}` for non-`:ready` states including `:degraded`. The plan should clarify whether the runner switches to `status/1` or stays with `check_health/1`.

### A4. Section Numbering Gap

Section 3.5 is missing. The plan jumps from 3.4 to 3.6.

---

## B. Underspecified Mechanisms

### B1. `use ToastTest.Suite` Macro Semantics Need More Detail

1. **CaseTemplate layering**: Who provides the `setup` block injecting `%{deployment: ..., client: ..., endpoint: ...}`? Does the suite's CaseTemplate `use ToastTest.Case` internally? Does `ToastTest.Case.setup` run first, then the suite's `setup_deployment` result is merged?

2. **Attribute quoting**: The plan says `@toast_suite __MODULE__` is injected, but inside a `__using__` macro, `__MODULE__` refers to the *test module*. The plan likely means `@toast_suite unquote(__MODULE__)` where `__MODULE__` is evaluated in the suite module context.

### B2. Suite Compilation Order Constraint

The plan says compile `suite.ex` first, then `*.ex` helpers, then `*_test.exs` files. This means suite modules must not depend on helper modules within the same folder. This constraint should be documented.

### B3. `expect_crash` Timeout Details

- What is the default timeout?
- Is it configurable per call?
- What happens when the timeout fires but the test is still running? The server could crash late, causing a false-positive abort.

### B4. Runtime Suite Binding Discovery

The `@toast_suite` attribute is compile-time only. At runtime, the runner needs to discover which suite each module belongs to. The macro should inject a function like `def __toast_suite__, do: ShellServer.Suite`, not just a module attribute.

### B5. ExUnit.Server Integration for Per-Suite Module Batching

This is the most critical underspecified mechanism. ExUnit.Server is a singleton. Modules are registered via `ExUnit.Server.modules_loaded/1`. There is no way to "unload" modules. The plan needs to resolve how per-suite module batching works:

- Stopping and restarting ExUnit between suites?
- Loading all modules upfront but running subsets?
- Bypassing ExUnit.Server entirely?

This is architecturally blocking for the suite system.

### B6. Interactive Test Module Compilation

`ToastTest.Interactive.run(Smoke.VersionTest, deployment: deployment)` requires the test module to be compiled. But `.exs` files in `suites/` are not auto-compiled by `iex -S mix`. The plan should specify how the module gets loaded.

---

## C. Friction With Existing Code

### C1. Runner Refactoring Scope Is Understated

The current runner is 870 lines forked from ExUnit.Runner. "Remove async, add suite loop" understates the work. The runner needs to: not use ExUnit.Server for module feeding (see B5), manage deployment lifecycle between suites, and reset ExUnit state between suites. This is closer to "rewrite the outer scheduling layer."

### C2. Application.put_env State Passing Is Pervasive

Multiple keys beyond `:__test_deployment__` need cleanup: `:__suite_deadline__`, `:__timeout_factor__`, `:__test_results__`, `:__test_diagnostics__`, `:__sanitizer_matching__`, `:__crash_matching__`. Section 4.8's cleanup list is incomplete.

### C3. `apply_toast_env` Removal Needs to Be Explicit

Phase 2 step 5 should be more specific: `apply_toast_env/1` is deleted entirely, replaced by a function that builds a keyword list.

### C4. Client Refactoring Scope

Every existing test call changes from `Toast.Client.create_collection(client, name)` to `Toast.Client.Collection.create(client, name)`. With only 3 test files in smoke suite, this is manageable. The plan should note the current test count to justify "migrate all at once."

### C5. ServerProcess Lacks Signal-Sending Capability

Phase 4 mentions controller-level operations but not `ServerProcess` changes. The actual `SIGKILL`/`SIGSTOP`/`SIGCONT` sending happens at the `ServerProcess` level via `exec.kill/2`. Phase 4 should include extending `ServerProcess` with signal-sending capabilities.

---

## D. Feasibility and Risk

### D1. Phase 3 Is Overloaded

14 steps covering suite system + REST client. Consider splitting:
- Phase 3a: REST Client refactoring (can be done independently)
- Phase 3b: Suite system (more complex, benefits from having client ready)

### D2. HealthMonitor Suspend Needs Status Field Update

The existing HealthMonitor has `:healthy` and `:unhealthy` states. Adding `:suspended` is straightforward, but `healthy?/1` needs to handle it. Should a suspended monitor report as healthy?

### D3. Coredump Path Discovery on CI

`/proc/sys/kernel/core_pattern` may point to an `apport` handler or `systemd-coredump` pipe on CI. The plan should note pipe-based core patterns require different discovery logic.

---

## E. Minor Issues

### E1. `with_api_version` vs. `with_api_prefix` — Unify

Two different function names for the same concept. Consider a single function: `with_api_version(client, 1)` → `/_arango/v1`, `with_api_version(client, "experimental")` → `/_arango/experimental`.

### E2. Success Criteria #5 Uses `--mode` Flag

Section 12 item 5: `mix toast smoke --mode cluster`. But the CLI design uses `--cluster`/`--single`. Fix the success criteria.

### E3. `Toast.Client.for_server` Dependency Direction

`Toast.Client.for_server(deployment, ...)` creates a dependency from client to deployment. Consider `Toast.Deployment.client(deployment, server_id)` instead.

---

## Summary of Actionable Items

**Must address:**
1. (B5) How per-suite module batching works with ExUnit.Server
2. (C1) Acknowledge runner refactoring scope
3. (A1) Specify what happens to `work_dir`, `crash_monitor`, and `endpoint` fields

**Should address:**
4. (B1) Clarify CaseTemplate layering
5. (B4) Specify runtime suite binding via function, not just attribute
6. (C2) Enumerate all Application.put_env keys needing cleanup
7. (C5) Add ServerProcess signal-sending as Phase 4 step
8. (D1) Consider splitting Phase 3

**Nice to have:**
9. (A4) Fix section numbering
10. (E1) Unify `with_api_version`/`with_api_prefix`
11. (E2) Fix success criteria #5
12. (E3) Decide dependency direction for `for_server`
