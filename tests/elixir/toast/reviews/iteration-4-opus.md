# Opus Review — Iteration 4

**Model:** claude-opus-4-6
**Generated:** 2026-02-23

---

## Overall Assessment

The plan is mature and well-integrated after three review iterations. The structural decisions are sound, the ExUnit.Server bypass is now well-specified, the callback injection boundary is clean, and the resilience testing design is thorough. What remains are subtle implementation-level gaps that would cause friction or surprise during development.

---

## 1. Agency Dump Lifecycle Ordering Problem

`stop_and_collect/1` needs to dump agency BEFORE shutting down agents. But `ClusterController.do_shutdown/2` is a single call. The plan doesn't specify how the agency dump hooks into the shutdown sequence. Options: controller does it internally, multi-step protocol, or dedicated pre-shutdown call.

## 2. `check_health` Needs Structured Returns

Current code conflates all non-`:ready` states into generic errors. Differentiated error messages for `:degraded` vs `:failed` need structured returns or pattern matching on `status/1`.

## 3. `on_event` Callback Invocation From Cluster Tasks

Cluster server starts happen in spawned tasks, not in the GenServer process. The plan says callback "is invoked within the controller GenServer's process" but this doesn't hold for cluster starts.

## 4. Versioned Module URL Construction

V1 modules must actively override client's api_version, not just ignore it. URL construction logic should be in a single place.

## 5. `setup_deployment` Context Merge Collisions

If suite returns `{:ok, %{client: custom_client}}`, it overwrites the default client. Should be documented as intentional override semantics.

## 6. `Code.compile_file` vs `Code.require_file`

Plan mentions both. Should consistently use `Code.compile_file/1` for interactive recompilation.

## 7. Port Allocator Should NOT Reset Between Suites

TIME_WAIT (60s on Linux) means recently-released ports may fail to bind. Safer to continue allocating from where the allocator left off.

## 8. `ExUnit.start()` Timing

Must be called in the mix task before any suite compilation, not in a test_helper.exs. Must use `autorun: false`.

## 9. Cluster ID Mapping Stable Across Restarts

ArangoDB persists server IDs in data directory. Mapping remains valid after restart. Worth noting.

## 10. `drain_remaining_modules` Must Be Rewritten

Current abort/drain logic reads from ExUnit.Server. New runner must iterate its own module list.

## 11. Suite Timeout Hierarchy Unspecified

Default timeout? Interaction with global deadline? Hierarchy: global deadline > suite timeout > test timeout.

## 12. `setup_deployment/1` Error Handling Unspecified

When it returns `{:error, reason}`, what happens? Tests marked as errored, deployment stopped.

## 13. `server_args` Semantics for Cluster

Does `server_args` apply to all servers or specific roles? Consider `coordinator_args`, `dbserver_args`.

## 14. `verify_crash` When Expectation Expired But Server Crashed

Should return `{:error, :expectation_expired}`, not `{:error, :not_crashed}`.

## 15. Optional Callback Dispatch

Use `function_exported?/3` for runtime dispatch of optional callbacks.

---

## Summary

**Must address:** 1 (agency dump lifecycle), 8 (ExUnit.start timing), 12 (setup_deployment error handling)
**Should address:** 3 (on_event from tasks), 7 (port allocator), 11 (timeout hierarchy), 13 (server_args roles)
**Worth noting:** 2, 4, 5, 6, 9, 10, 14, 15
