Now I have comprehensive context. Let me write the section content.

# Section 07: Resilience Testing

## Overview

This section implements the resilience testing layer: the `expect_crash`/`verify_crash` mechanism for failure-point-triggered crashes, failure point management via ArangoDB's debug API, cluster-internal server ID mapping, and a proof-of-concept resilience test suite. These features enable tests to deliberately trigger server crashes through application logic (failure points) and verify the crash occurred, without the health monitoring system treating these as unexpected failures.

**Dependencies**: This section builds on:
- **Section 03 (REST Client)**: The failure point API uses the REST client to call ArangoDB debug endpoints
- **Section 05 (Runner)**: The runner's between-test health check (`:ready`/`:degraded`/`:failed` status) is the safety net when `expect_crash` timeouts expire
- **Section 06 (Server Control)**: The `intentional` flag, `:suspend`/`:resume` on HealthMonitor, server state tracking, `:degraded` status, and control operations (`stop_server`, `kill_server`, `restart_server`, etc.) are prerequisites

This section assumes the following from prior sections are already implemented:
- Deployment struct with `controller` PID and `mode` field
- Controller GenServer with per-server state tracking (`operational_state`, `intentional` flag)
- HealthMonitor with `:suspend`/`:resume` messages and `:suspended` status
- `Toast.Client` with `get/post/put/delete` operations
- `Toast.Deployment` server control operations (`stop_server`, `kill_server`, `restart_server`, etc.)
- `Toast.Deployment.client/2` for creating clients to specific servers
- ClusterController with `:degraded` deployment status

---

## Tests

Write tests BEFORE implementing. All tests are unit tests that mock external dependencies (erlexec, HTTP). They run with `mix test` and do not require a running ArangoDB instance.

### 7.1 expect_crash / verify_crash Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/deployment/expect_crash_test.exs`

```elixir
defmodule Toast.Deployment.ExpectCrashTest do
  @moduledoc """
  Tests for the expect_crash/verify_crash mechanism.

  These tests verify that:
  - expect_crash suspends monitoring and returns :ok
  - verify_crash confirms whether the expected crash occurred (keyed by server_id)
  - Auto-clearing after timeout restores monitoring
  - Late crashes after timeout are treated as unexpected
  """
  use ExUnit.Case, async: true

  # Test: expect_crash/3 returns :ok and suspends monitoring
  # Test: expect_crash/3 with timeout: option overrides default 30s
  # Test: at most one pending expectation per server (second call returns {:error, :already_expected})
  # Test: verify_crash/3 returns {:ok, crash_info} when server crashed as expected
  # Test: verify_crash/3 returns {:error, :not_crashed} when server still running
  # Test: verify_crash/3 returns {:error, :timeout} after timeout expires
  # Test: expect_crash auto-clears after timeout and resumes monitoring
  # Test: late crash after timeout clearing treated as unexpected (triggers on_crash)
  # Test: on_crash callback is NOT invoked when expect_crash is set
end
```

### 7.2 Failure Point Management Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/deployment/failure_point_test.exs`

```elixir
defmodule Toast.Deployment.FailurePointTest do
  @moduledoc """
  Tests for failure point management via ArangoDB debug API.

  Uses Mox to mock the HTTP client. Verifies correct endpoint construction,
  role-based targeting, and error handling for non-debug builds.
  """
  use ExUnit.Case, async: true

  # Test: set_failure_point/3 calls PUT /_admin/debug/failat/{name} on target server
  # Test: clear_failure_point/3 calls DELETE /_admin/debug/failat/{name}
  # Test: clear_all_failure_points/1 calls DELETE /_admin/debug/failat on all servers
  # Test: role-based targeting for failure points (e.g., role: :dbserver)
  # Test: returns {:error, :not_supported} when server returns 404 (no failure point support)
end
```

### 7.3 Server ID Mapping Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/deployment/server_id_mapping_test.exs`

```elixir
defmodule Toast.Deployment.ServerIdMappingTest do
  @moduledoc """
  Tests for cluster-internal server ID mapping.

  Verifies the mapping between Toast IDs (e.g., "dbserver-0") and
  cluster-internal IDs (e.g., "PRMR-abc123") fetched from the agency.
  """
  use ExUnit.Case, async: true

  # Test: cluster_id/2 returns cluster-internal ID for toast ID
  # Test: server_by_cluster_id/2 returns server info for cluster-internal ID
  # Test: mapping fetched from /_admin/cluster/health after cluster formation
  # Test: mapping cached in ClusterController state (no repeated HTTP calls)
  # Test: mapping stable across server restarts (data dir preserved, same ID)
  # Test: control operations accept cluster_id: targeting
end
```

### 7.4 Resilience Suite Integration Test Stubs

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/resilience/suite.ex`

```elixir
defmodule Resilience.Suite do
  @moduledoc """
  Suite for resilience testing: deliberate server manipulation and recovery.

  Requires cluster mode since resilience tests exercise multi-server scenarios
  (dbserver crashes, coordinator failover, etc.).
  """
  use ToastTest.Suite,
    mode: :cluster,
    cluster_dbservers: 3,
    cluster_coordinators: 2
end
```

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/resilience/test_server_lifecycle.exs`

```elixir
defmodule Resilience.ServerLifecycleTest do
  @moduledoc """
  Proof-of-concept resilience tests demonstrating server control operations.
  Each test restores the deployment to a healthy state before finishing.
  """
  use Resilience.Suite

  # Test: stop and restart a dbserver
  # Test: pause and resume a coordinator
  # Test: kill a dbserver and verify recovery
  # Test: expect_crash with failure point, then verify and restart
  # Test: health monitoring behaves correctly during deliberate actions
end
```

---

## Implementation

### 7.5 expect_crash / verify_crash Mechanism

#### Design

The `expect_crash`/`verify_crash` API handles crashes triggered indirectly by application logic (failure points), where the test cannot use `kill_server` because the crash is not caused by a signal but by ArangoDB hitting a failure point during request processing.

**Flow**:

1. Test calls `Toast.Deployment.expect_crash(deployment, server_id)` -- returns `:ok`
2. Controller suspends health monitoring for that server
3. Controller records an "expected crash" entry keyed by server_id with a timer
4. Test triggers the action that causes the crash (e.g., a request that hits the failure point)
5. Server crashes -- erlexec delivers `:DOWN` message to `ServerProcess`, which forwards `{:server_crashed, ...}` to the controller
6. Controller sees the `expect_crash` entry, records the crash as intentional, stores crash_info, does NOT invoke `:on_crash` callback
7. Test calls `Toast.Deployment.verify_crash(deployment, server_id)` -- returns `{:ok, crash_info}`
8. Test restarts the server: `Toast.Deployment.restart_server(deployment, server_id)`

**Simplification**: At most one pending expectation per server (concurrent ops serialized by GenServer mailbox). Server ID is sufficient for identification — a ref adds indirection without value.

**Auto-clearing on timeout**: If the crash does not happen within the timeout (default 30s), a `Process.send_after` timer fires in the controller. The controller clears the expectation, resumes health monitoring, and marks the server as "no longer expecting a crash". Any subsequent crash is treated as unexpected.

**Edge case -- late crash**: If the crash arrives just after the timeout clears the expectation, the controller treats it as an unexpected crash (correct behavior). The runner's between-test `:ready` health check catches this, aborting the suite with a clear error.

#### Files to Modify

**`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex`**

Add these public functions:

```elixir
@doc """
Mark a server as expected to crash. Suspends health monitoring.
At most one pending expectation per server.

Options:
  - timeout: milliseconds before auto-clearing (default: 30_000)
"""
@spec expect_crash(t(), server_target(), keyword()) :: :ok | {:error, term()}
def expect_crash(deployment, server_id_or_target, opts \\ [])

@doc """
Verify that an expected crash occurred.
Returns {:ok, crash_info} if the server crashed as expected,
{:error, :not_crashed} if still running,
{:error, :timeout} if the expectation timed out and was cleared.

Options:
  - timeout: milliseconds to wait for crash (default: 5_000)
"""
@spec verify_crash(t(), server_target(), keyword()) :: {:ok, map()} | {:error, atom()}
def verify_crash(deployment, server_id_or_target, opts \\ [])
```

Both functions delegate to the controller via `GenServer.call`. Server ID is the key — no ref needed since at most one expectation per server.

**`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/single_server_controller.ex`** and **`cluster_controller.ex`**

Add to controller state:

```elixir
# In the state map:
expected_crashes: %{}  # %{server_id => %{timer: timer_ref, crash_info: nil | map()}}
```

Add `handle_call` clauses:

- `{:expect_crash, server_id, timeout}` -- create expectation entry, suspend health monitor, start timer, return `:ok`. Returns `{:error, :already_expected}` if server already has a pending expectation.
- `{:verify_crash, server_id, timeout}` -- look up by server_id, return crash_info or `:not_crashed`. If crash hasn't occurred yet, can wait up to timeout.
- Handle `:expect_crash_timeout` info message -- clear expectation, resume monitoring

Modify the `handle_info({:server_crashed, ...})` clause:

- Before invoking the crash callback, check if the crashing server has an active `expect_crash` entry
- If yes: store crash_info in the entry, do NOT invoke `:on_crash`, keep state as `:degraded` (not `:failed`)
- If no: existing behavior (invoke `:on_crash`, set status to `:failed`)

#### Controller State Machine Changes

The existing crash handling in the controller needs a check inserted:

```
{:server_crashed, server_id, crash_info} arrives
  -> check expected_crashes[server_id]
     -> if found: store crash_info in entry, set server state to {:crashed, intentional: true}
     -> if not found: existing behavior (set :failed, invoke on_crash)
```

For `expect_crash_timeout`:

```
:expect_crash_timeout for server_id
  -> delete expected_crashes[server_id]
  -> resume health monitoring for server_id
  -> if server is still running: no further action
  -> if server crashed between expect and timeout: already handled above
```

### 7.6 Failure Point Management

#### Design

Failure points are ArangoDB's mechanism for injecting faults at specific code locations. They are only available in debug builds. The API is thin REST wrappers.

#### File to Create

**`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/failure_point.ex`**

```elixir
defmodule Toast.Deployment.FailurePoint do
  @moduledoc """
  Manage failure points on ArangoDB servers via the debug API.

  Failure points are only available in debug/maintainer builds.
  Operations return {:error, :not_supported} when the server
  does not support failure points (HTTP 404 from the debug endpoint).
  """

  @doc "Set a failure point on the target server(s)."
  @spec set(Toast.Deployment.t(), server_target(), String.t()) :: :ok | {:error, term()}
  def set(deployment, target, name)

  @doc "Clear a specific failure point on the target server(s)."
  @spec clear(Toast.Deployment.t(), server_target(), String.t()) :: :ok | {:error, term()}
  def clear(deployment, target, name)

  @doc "Clear all failure points on all servers in the deployment."
  @spec clear_all(Toast.Deployment.t()) :: :ok | {:error, term()}
  def clear_all(deployment)
end
```

The implementation:

- `set/3` calls `PUT /_admin/debug/failat/{name}` on the target server(s)
- `clear/3` calls `DELETE /_admin/debug/failat/{name}` on the target server(s)
- `clear_all/1` calls `DELETE /_admin/debug/failat` on every server in the deployment

Target resolution reuses the same mechanism as server control operations (Section 06): a specific server ID, `role: :dbserver`, `role: :coordinator, index: 0`, or `cluster_id: "PRMR-xxx"`. The target is resolved to one or more server IDs, and the REST call is made to each.

Each REST call uses `Toast.Deployment.client/2` to get a client for the specific server, then calls the debug endpoint. HTTP 404 is interpreted as `:not_supported` (server not built with failure point support). HTTP 200 is success.

These functions are also exposed as convenience wrappers on `Toast.Deployment`:

**`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex`**

```elixir
@doc "Set a failure point on target server(s). See Toast.Deployment.FailurePoint."
defdelegate set_failure_point(deployment, target, name), to: Toast.Deployment.FailurePoint, as: :set

@doc "Clear a specific failure point on target server(s)."
defdelegate clear_failure_point(deployment, target, name), to: Toast.Deployment.FailurePoint, as: :clear

@doc "Clear all failure points on all servers."
defdelegate clear_all_failure_points(deployment), to: Toast.Deployment.FailurePoint, as: :clear_all
```

### 7.7 Process History *(Moved from Section 04)*

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/process_history.ex`

ProcessHistory is only needed for resilience tests with server restarts, where diagnostics (sanitizer logs, core dumps, named with OS PID) must be correlated to specific server instances across restarts. The existing CrashMatcher/SanitizerMatcher already use timestamps for basic correlation, which is sufficient for Phase 3b. ProcessHistory adds precise PID-to-server-instance mapping.

A GenServer that records process lifecycle events. It is provided as the `:on_event` callback when the runner starts a deployment:

```elixir
Toast.Deployment.start(:cluster,
  on_crash: &ToastTest.CrashMonitor.handle_crash/2,
  on_event: &ToastTest.ProcessHistory.handle_event/1
)
```

The callback receives tuples:
- `{:server_started, server_id, os_pid, timestamp}`
- `{:server_stopped, server_id, os_pid, exit_info, timestamp}`
- `{:server_crashed, server_id, os_pid, crash_info, timestamp}`

The `handle_event/1` function calls `GenServer.cast` internally (non-blocking). The GenServer maintains a log of events keyed by OS PID and timestamped. This enables:
- Correlating sanitizer log files (named with OS PID) to specific server instances
- Correlating core dumps (named with OS PID) to server instances
- Determining which test was running when a server crashed

The process is started as part of the test framework's supervision (registered as `ToastTest.ProcessHistory`). It is cleared between suites as part of inter-suite state cleanup.

### 7.8 Cluster-Internal Server ID Mapping

#### Design

ArangoDB clusters assign internal IDs to servers during cluster formation (e.g., `PRMR-abc123` for primary/dbservers, `CRDN-xyz456` for coordinators, `AGNT-...` for agents). These IDs are determined by `ServerState.cpp` when a server registers with the agency. They are stable across restarts as long as the data directory is preserved (the ID is stored in the data directory).

Tests that need to interact with cluster-internal concepts (e.g., "crash the leader of shard X") need to map between Toast IDs (`dbserver-0`) and cluster-internal IDs (`PRMR-abc123`).

#### Data Source

The mapping is fetched from `GET /_admin/cluster/health` on any coordinator. The response contains entries like:

```json
{
  "Health": {
    "PRMR-abc123": {
      "Endpoint": "tcp://127.0.0.1:8530",
      "Role": "DBServer",
      "Status": "GOOD",
      "ShortName": "DBServer0001"
    }
  }
}
```

The endpoint in the response is matched against the known server endpoints in the controller state to establish the Toast ID <-> cluster-internal ID mapping.

#### Files to Modify

**`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/cluster_controller.ex`**

Add to state:

```elixir
# In the state map:
cluster_id_map: %{}  # %{toast_id => cluster_internal_id}
reverse_id_map: %{}  # %{cluster_internal_id => toast_id}
```

After cluster health check succeeds (all servers registered and healthy), fetch the mapping:

```elixir
defp fetch_cluster_id_mapping(state) do
  # GET /_admin/cluster/health from first coordinator
  # Parse response to build bidirectional mapping
  # Match endpoints from response to known server endpoints in state.servers
end
```

Add `handle_call` clauses:

- `{:cluster_id, toast_id}` -- look up in `cluster_id_map`
- `{:server_by_cluster_id, cluster_internal_id}` -- look up in `reverse_id_map`, return server info

**`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex`**

Add public functions:

```elixir
@doc """
Get the cluster-internal ID for a Toast server ID.
Only available for cluster deployments.
"""
@spec cluster_id(t(), String.t()) :: {:ok, String.t()} | {:error, term()}
def cluster_id(%__MODULE__{mode: :cluster, controller: pid}, toast_id)

@doc """
Get server info by cluster-internal ID.
Only available for cluster deployments.
"""
@spec server_by_cluster_id(t(), String.t()) :: {:ok, map()} | {:error, term()}
def server_by_cluster_id(%__MODULE__{mode: :cluster, controller: pid}, cluster_internal_id)
```

For single-server deployments, both functions return `{:error, :not_a_cluster}`.

#### Integration with Control Operations

The server targeting system (already implemented in Section 06 for role-based targeting) is extended to accept `cluster_id:` targeting:

```elixir
Toast.Deployment.stop_server(deployment, cluster_id: "PRMR-abc123")
```

This resolves through `server_by_cluster_id/2` to find the Toast ID, then delegates to the normal control operation path. The target resolution function needs to handle one additional clause:

```elixir
defp resolve_target(state, cluster_id: id) do
  case Map.get(state.reverse_id_map, id) do
    nil -> {:error, {:unknown_cluster_id, id}}
    toast_id -> {:ok, [toast_id]}
  end
end
```

### 7.9 Proof-of-Concept Resilience Suite

#### File Structure

```
suites/resilience/
  suite.ex                    # Suite definition (cluster mode, 3 dbservers, 2 coordinators)
  test_server_lifecycle.exs   # Server stop/restart/kill/pause/resume tests
```

#### Suite Definition

The resilience suite explicitly requires cluster mode because resilience tests exercise multi-server scenarios. It requests 3 dbservers and 2 coordinators to enable meaningful failover and recovery testing.

**`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/resilience/suite.ex`**

```elixir
defmodule Resilience.Suite do
  use ToastTest.Suite,
    mode: :cluster,
    cluster_dbservers: 3,
    cluster_coordinators: 2
end
```

#### Test Design Principles

Each resilience test follows a strict pattern:
1. Verify deployment is healthy at start
2. Perform the deliberate manipulation
3. Assert the expected behavior
4. Restore the deployment to a healthy state (restart any stopped/killed servers)
5. Verify deployment is healthy at end

This pattern is enforced by the runner's between-test health check (from Section 05), which rejects `:degraded` status. Tests that fail to restore servers will cause a clear abort rather than corrupting subsequent tests.

#### Example Test Scenarios

**`/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/resilience/test_server_lifecycle.exs`**

The test file should demonstrate these scenarios:

1. **Stop and restart a dbserver**: Call `stop_server`, verify status is `:degraded`, verify the dbserver is not reachable, call `restart_server`, verify status returns to `:ready`.

2. **Pause and resume a coordinator**: Call `pause_server` (SIGSTOP), verify the coordinator stops responding to HTTP, call `resume_server` (SIGCONT), verify it becomes responsive again.

3. **Kill and recover a dbserver**: Call `kill_server` (SIGKILL), verify the server is gone, call `start_server`, wait for it to become healthy, verify deployment is `:ready`.

4. **Expected crash via failure point**: Call `set_failure_point(deployment, role: :dbserver, index: 0, "crash-after-commit")`. Call `expect_crash(deployment, "dbserver-0")` (returns `:ok`). Trigger the failure point by performing an operation. Call `verify_crash(deployment, "dbserver-0")` to confirm (returns `{:ok, crash_info}`). Call `clear_all_failure_points(deployment)`. Call `restart_server(deployment, "dbserver-0")`. Verify deployment health.

5. **Health monitoring during deliberate actions**: Verify that during `stop_server`/`kill_server`/`expect_crash`, the health monitor does not fire spurious `:server_unhealthy` messages. Verify that after `restart_server`/`resume_server`, health monitoring resumes and detects when the server becomes healthy again.

---

## Server Target Resolution

Throughout this section and Section 06, control operations and failure point management accept a flexible `server_target()` type. This is worth documenting explicitly since it is the common interface for all operations.

The target type is:

```elixir
@type server_target ::
  String.t()                              # Toast ID: "dbserver-0"
  | [role: atom()]                        # All of role: [role: :dbserver]
  | [role: atom(), index: non_neg_integer()] # Specific role index: [role: :coordinator, index: 0]
  | [cluster_id: String.t()]              # Cluster-internal ID: [cluster_id: "PRMR-abc123"]
```

Target resolution is implemented as a private function in the controller modules. For `ClusterController`, all four forms are supported. For `SingleServerController`, only Toast ID is relevant (the single server's ID).

---

## Migration Notes (Phase 4, Steps 4, 8-10)

This section corresponds to Phase 4 steps:

- **Step 4**: Implement `expect_crash`/`verify_crash` for failure-point-triggered crashes
- **Step 8**: Implement cluster-internal server ID mapping (`cluster_id/2`, `server_by_cluster_id/2`)
- **Step 9**: Implement ProcessHistory observer (moved from section-04, deferred from Phase 3b)
- **Step 10**: Implement failure point management (`set_failure_point`, `clear_failure_point`, `clear_all_failure_points`) with role/server targeting
- **Step 11**: Write resilience test suite as proof-of-concept

**Verification criteria** (from the migration plan): resilience test suite passes -- servers stopped, killed, restarted without false alerts. Expected crash verification works with failure points. Cluster-internal ID mapping enables targeted operations. ProcessHistory correctly correlates diagnostics files to server instances across restarts.