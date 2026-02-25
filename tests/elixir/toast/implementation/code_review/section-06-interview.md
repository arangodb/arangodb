# Section 06 Code Review Interview

## Auto-fixes applied
- **H1 stop handlers for paused/killed**: Added `handle_call({:stop, ...})` for `:paused` (SIGCONT then stop flow) and `:killed` (return :ok, already dead)
- **H2 arg accumulation bug**: Stored `original_args` in init, relaunch merges from `original_args` not `args`
- **L2 kill from paused**: Changed kill guard from `:running` to `status in [:running, :paused]`
- **L3 :shutdown in DOWN guard**: Changed `reason != :normal` to `reason not in [:normal, :shutdown]` in both controllers
- **L4 GenServer call timeout**: Changed restart/start client timeout from 60s to 65s for headroom
- **M3 degraded guard**: HealthMonitor restart guard now checks `state.status in [:ready, :degraded]` in both controllers

## Interview decisions

### 1. Role-based targeting (M1)
**Decision: Defer to section-07**

Role-based targeting (`role: :dbserver`, `cluster_id: "PRMR-xxx"`) will be implemented alongside expect_crash and cluster_id mapping in section-07 (Resilience). Only direct server_id strings accepted for now.

### 2. restart_server on paused servers (H3)
**Decision: Handle all non-running states**

restart_server now works from any operational state:
- `:running` → graceful stop then relaunch
- `:paused` → SIGKILL (since process is frozen) then relaunch
- `:stopped`/`:killed`/`:crashed` → just relaunch

Applied to both SingleServerController and ClusterController.

## Items let go
- M2: @spec annotations — per project style, not adding to code we didn't author
- M4: Stale server struct in cluster restart — theoretical, low practical impact
- M5: derive_cluster_status edge case — intentional+crashed state shouldn't normally occur
- M6: Test coverage — functional coverage sufficient, can extend later
- L1: Code duplication between controllers — refactoring out of scope
