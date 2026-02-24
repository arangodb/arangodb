# Opus Review — Iteration 3

**Model:** claude-opus-4-6
**Generated:** 2026-02-23

---

## Overall Assessment

The plan has reached a high level of maturity. The previous two review iterations addressed the structural issues (struct-as-handle, callback injection, ExUnit.Server bypass, signal-type awareness, inter-suite cleanup). What remains are implementation-level concerns: a few internal contradictions in the plan text, feasibility risks around the ExUnit.Server bypass that need deeper analysis, and some missing operational details that will bite during implementation.

---

## 1. ExUnit.Server Bypass: The Plan Underspecifies the Hard Part

The per-test internals depend on `module.__ex_unit__()` which is a function on the compiled module itself, not fetched from ExUnit.Server. The bypass should work. However, `use ExUnit.Case` automatically calls `ExUnit.Server.add_module/2` during compilation — modules will register with ExUnit.Server regardless. The plan should explicitly state approach: accept that modules register but never read from ExUnit.Server. `ExUnit.start()` must still be called once; `modules_loaded/1` should NOT be called.

## 2. `test_*.exs` Naming Convention Creates Tooling Friction

Most Elixir editor plugins recognize `*_test.exs` for syntax highlighting, "run test" actions, etc. `test_*.exs` breaks these integrations. The `.exs` vs `.ex` distinction already separates tests from helpers. Recommend reconsidering or acknowledging tooling friction.

## 3. Suite Module IS the CaseTemplate

"Generates a CaseTemplate" suggests a separate module. In reality, `use ToastTest.Suite` turns the suite module itself into a CaseTemplate by injecting `use ExUnit.CaseTemplate` and a `__using__` callback. Should be stated explicitly.

## 4. `setup_deployment/1` Timing Is Ambiguous

Is it once per suite (after deployment starts) or per test (via CaseTemplate)? The naming suggests suite-level. The CaseTemplate layering description suggests per-test. Need to resolve: suite-level setup runs once and result is stored; per-test setup uses standard ExUnit `setup` blocks.

## 5. `:degraded` Rejection May Be Too Strict

Aborting 199 tests because one test's cleanup failed is painful during development. Consider configurable behavior (`on_degraded: :abort | :recover`, defaulting to `:abort` for CI, `:recover` for local).

## 6. `on_event` Callback Could Block Controller

If the callback does a synchronous GenServer.call, it blocks the controller. Document that callbacks must be non-blocking, or invoke via `spawn`/`Task.start`.

## 7. Incomplete API Versions Risk for Infrastructure Operations

Latest API version may be "incomplete" (only contains endpoints with breaking changes). Global default set to an incomplete version would break infrastructure operations (collection creation, etc. returns 404). Recommend global default should always be a complete version or `nil`.

## 8. Formatter Lifecycle: Cross-Suite Stats Aggregation

With per-suite EventManager + RunnerStats, cross-suite aggregate stats need a separate accumulator. The runner should extract stats after each suite and merge into an aggregate.

## 9. Deployment Handle Delivery to `ToastTest.Case.setup`

The plan eliminates env vars for deployment *configuration* but Section 4.8 still lists `:__test_deployment__` as a key to clean up. The plan should explicitly state that `Application.put_env(:toast, :__test_deployment__, deployment)` remains the mechanism.

## 10. `stop_and_collect/1` Return Type Inconsistency

Plan says `{:ok, diagnostics}`. Existing code returns bare `diagnostics | nil`.

## 11. `:auto` Mode Resolution Location

`Toast.Deployment.start/2` accepts `:single_server | :cluster`. The runner must resolve `:auto` to an actual mode before calling `start/2`.

## 12. Cluster ID Mapping: Storage and Fetch

Where is the mapping stored? (ClusterController state.) How is it populated? (Agency HTTP API after cluster formation.) What happens if agency is unavailable when queried?

## 13. Signal Delivery via erlexec

`:exec.kill/2` used for SIGKILL/SIGSTOP/SIGCONT. SIGSTOP does not trigger erlexec exit monitoring. Three-hop GenServer latency is acceptable for low-frequency operations.

## 14. `restart_server` `opts` Semantics

What does "different configuration" mean? Recommend: additional/override CLI arguments merged with original launch spec. Port, data directory, binary immutable across restarts.

## 15. Missing Success Criterion for Intra-Suite Isolation

No criterion for: a test failure within a suite does not affect other suites.

---

## Summary

**Must address:** 1 (ExUnit.Server bypass detail), 3 (suite IS CaseTemplate), 9 (deployment handle delivery), 11 (auto mode resolution)

**Should address:** 4 (setup_deployment timing), 7 (incomplete API version risk), 8 (cross-suite stats), 14 (restart_server opts)

**Consider:** 2 (test_*.exs naming), 5 (degraded recovery), 6 (on_event blocking), 10 (return type), 12 (cluster ID mapping), 13 (signal delivery), 15 (success criterion)
