# Section 06 Code Review

## Summary

The implementation covers the core plan requirements: ServerProcess signal extensions (kill/pause/resume/relaunch), ServerInstance struct extension, controller state tracking with signal-type awareness, and Deployment API delegation. Several significant correctness issues found: missing stop handlers for new states, arg accumulation bug in relaunch, restart_server unable to handle paused servers.

## Findings

### H1: ServerProcess.stop/2 has no handler for :paused or :killed states (High)
No `handle_call({:stop, ...})` clauses for `:paused` or `:killed`. During deployment shutdown (stop_server_process), calling `ServerProcess.stop/2` on a paused process causes FunctionClauseError crash.
File: server_process.ex:168-183
Suggestion: Add handlers — paused: SIGCONT then stop flow; killed: return :ok (already dead).

### H2: relaunch/2 permanently mutates args — accumulation across restart cycles (High)
`state.args ++ extra_args` overwrites `args` in state. Second relaunch appends again, causing `original ++ extra1 ++ extra2`. Plan says merge with original base args.
File: server_process.ex:214-215
Suggestion: Store `original_args` in init, always merge from that.

### H3: restart_server cannot restart a :paused server (High)
Restart only stops if operational_state == :running. A paused server skips stop, then relaunch fails because ServerProcess status is :paused (not in [:stopped, :killed, :crashed]).
File: single_server_controller.ex:209-234, cluster_controller.ex:244-275
Suggestion: Handle paused state in restart flow (resume then stop, or kill then relaunch).

### M1: No role-based or keyword targeting in controllers (Medium)
Plan specifies role-based (`role: :dbserver`), role+index, and cluster_id targeting with `resolve_targets/2`. Not implemented — only direct server_id string accepted.
File: cluster_controller.ex (missing resolve_targets)

### M2: Missing @spec on new public APIs (Medium)
Plan provides explicit @spec for all new functions. 12 new public functions lack @spec.

### M3: HealthMonitor restart guard only checks :ready, misses :degraded (Medium)
`state.status == :ready` guard means HM crashes during :degraded state are silently ignored. Running servers lose health monitoring.
File: single_server_controller.ex:299, cluster_controller.ex:343
Suggestion: Change to `state.status in [:ready, :degraded]`.

### M4: resume_server_health_monitor uses stale server struct in cluster restart (Medium)
In ClusterController restart/start, `server` variable is bound before the stop/relaunch cycle but used after for resume_server_health_monitor.
File: cluster_controller.ex:260, 289

### M5: derive_cluster_status lets intentional crash fall to :ready (Medium)
If a server has operational_state: :crashed with intentional: true, it passes the first check, and if no other servers are stopped/killed/paused, falls through to `true -> :ready`.
File: cluster_controller.ex:785-795

### M6: Test coverage superficial (Medium)
Plan specifies ~50 test cases. Implementation provides 23, mostly basic happy paths. Missing: signal-type awareness, race conditions, deployment status derivation, callback tests.

### L1: Code duplication between controllers (Low)
Control operation handlers nearly identical in both controllers. Signal-type awareness duplicated verbatim.

### L2: ServerProcess.kill/1 cannot kill a :paused process (Low)
Guard only matches :running. At OS level, SIGKILL on a frozen process is valid. Users must resume before kill.
File: server_process.ex:185
Suggestion: Allow kill from :paused.

### L3: No :shutdown handling in HealthMonitor DOWN guard (Low)
Guard is `reason != :normal`. Plan says `:normal` or `:shutdown` are intentional.
File: single_server_controller.ex:298, cluster_controller.ex:341

### L4: GenServer call timeouts for restart/start race with internal timeouts (Low)
Client uses 60_000, internal health check uses 60_000. Existing deploy/shutdown use timeout + 5_000 for headroom.
