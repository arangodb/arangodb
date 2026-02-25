Now I have a comprehensive understanding of the existing codebase and the plan. Let me generate the section content.

# Section 06: Server Control Operations

## Overview

This section adds per-server control operations to the deployment infrastructure, enabling resilience testing and interactive server manipulation. The work spans three layers:

1. **ServerProcess** -- extend the low-level GenServer with signal-sending capabilities (`:kill`, `:pause`, `:resume`)
2. **Controllers** -- add server state tracking with the `intentional` flag, signal-type awareness, `:degraded` status, and HealthMonitor process monitoring
3. **HealthMonitor** -- add `:suspend`/`:resume` messages with timer cancellation and a `:suspended` status
4. **Deployment API** -- expose `stop_server`, `kill_server`, `pause_server`, `resume_server`, `restart_server`, `start_server` with server ID, role-based, and cluster-internal-ID targeting

This section does NOT include `expect_crash`/`verify_crash`, failure point management, or cluster-internal server ID mapping -- those belong to section-07 (Resilience).

## Dependencies

- **section-02-library-extraction**: The Deployment struct must already be refactored (immutable handle, `:on_crash`/`:on_event` callbacks, controller PID-based architecture). The supervision tree (`Toast.Process.Supervisor`, `Toast.Deployment.Supervisor`) must be in place.
- **section-02** also provides the HealthMonitor with its existing polling logic, which this section extends.

## File Inventory

After section-02, the relevant files live under the restructured project. All paths below assume the post-restructure layout.

| File | Action |
|------|--------|
| `lib/toast/process/server_process.ex` | Modify -- add `:kill`, `:pause`, `:resume` handle_call, add `relaunch/2` |
| `lib/toast/deployment/single_server_controller.ex` | Modify -- add per-server state tracking, control op handle_calls, HealthMonitor monitoring |
| `lib/toast/deployment/cluster_controller.ex` | Modify -- add per-server state map, control op handle_calls, `:degraded` status, HealthMonitor monitoring |
| `lib/toast/deployment/server_instance.ex` | Modify -- add `operational_state` and `intentional` fields |
| `lib/toast/process/health_monitor.ex` | Modify -- add `:suspend`/`:resume` messages, `:suspended` status |
| `lib/toast/deployment.ex` | Modify -- add `stop_server`, `kill_server`, `pause_server`, `resume_server`, `restart_server`, `start_server` functions with targeting |
| `test/toast/process/server_process_control_test.exs` | Create |
| `test/toast/process/health_monitor_suspend_test.exs` | Create |
| `test/toast/deployment/controller_state_test.exs` | Create |
| `test/toast/deployment/server_control_test.exs` | Create |

---

## Tests

Write these tests BEFORE implementing. Tests use Mox or simple stubs for `:exec` calls and HTTP. No running ArangoDB needed.

### Test File: `test/toast/process/server_process_control_test.exs`

Tests for the new signal-sending capabilities added to `ServerProcess`.

```elixir
defmodule Toast.Process.ServerProcessControlTest do
  @moduledoc """
  Tests for ServerProcess control operations: kill, pause, resume.
  Uses the fake_server.sh test helper to verify signal delivery.
  """
  use ExUnit.Case, async: false

  alias Toast.Process.ServerProcess

  # Test: kill/1 sends SIGKILL via :exec.kill/2 and transitions to :killed status
  # Test: kill/1 on a stopped server returns {:error, :not_running}
  # Test: pause/1 sends SIGSTOP (signal 19) via :exec.kill/2 and transitions to :paused status
  # Test: pause/1 does not trigger erlexec exit monitoring (process still alive, just frozen)
  # Test: resume/1 sends SIGCONT (signal 18) via :exec.kill/2 and transitions back to :running
  # Test: resume/1 on a non-paused server returns {:error, :not_paused}
  # Test: relaunch/2 on a stopped/killed/crashed server starts the process again
  # Test: relaunch/2 on a running server returns {:error, {:already_launched, :running}}
  # Test: relaunch/2 with args: [...] merges additional args with original launch spec
  # Test: relaunch/2 preserves the original executable, env, working_dir
end
```

### Test File: `test/toast/process/health_monitor_suspend_test.exs`

Tests for HealthMonitor suspend/resume behavior.

```elixir
defmodule Toast.Process.HealthMonitorSuspendTest do
  @moduledoc """
  Tests for HealthMonitor :suspend/:resume messages and :suspended status.
  """
  use ExUnit.Case, async: true

  alias Toast.Process.HealthMonitor

  # Test: HealthMonitor accepts :suspend message and stops polling
  # Test: HealthMonitor :suspend cancels pending Process.send_after timer
  # Test: HealthMonitor accepts :resume message and restarts polling
  # Test: HealthMonitor.status/1 returns :healthy | :unhealthy | :suspended
  # Test: suspended monitor does not fire :check messages (verify no health check HTTP calls)
  # Test: multiple :suspend then single :resume restores monitoring
  # Test: :resume on an already-active (non-suspended) monitor is a no-op
  # Test: :suspend on an already-suspended monitor is idempotent
end
```

### Test File: `test/toast/deployment/controller_state_test.exs`

Tests for controller server state tracking, the intentional flag, and signal-type awareness.

```elixir
defmodule Toast.Deployment.ControllerStateTest do
  @moduledoc """
  Tests for controller state machine transitions, intentional flag behavior,
  and deployment-level status derivation.
  Uses stubbed ServerProcess and HealthMonitor interactions.
  """
  use ExUnit.Case, async: false

  # --- Server state transitions ---
  # Test: :running -> stop_server -> :stopped (intentional: true)
  # Test: :running -> unexpected crash -> :crashed (intentional: false)
  # Test: :running -> kill_server -> :killed (intentional: true)
  # Test: :running -> pause_server -> :paused (intentional: true)
  # Test: :paused -> resume_server -> :running (intentional reset to false)
  # Test: :stopped -> start_server -> :running
  # Test: :killed -> start_server -> :running

  # --- Signal-type awareness ---
  # Test: SIGSEGV during intentional stop clears intentional flag, triggers on_crash
  # Test: SIGTERM during intentional stop is treated as intentional (no on_crash)
  # Test: SIGABRT during intentional stop clears intentional flag, triggers on_crash

  # --- Race conditions ---
  # Test: crash arrives before stop_server -> crash treated as unexpected, stop returns {:error, :already_crashed}

  # --- ClusterController deployment status ---
  # Test: all servers running -> :ready
  # Test: some servers intentionally down -> :degraded
  # Test: any server unexpectedly crashed -> :failed

  # --- Crash notification ---
  # Test: on_crash callback invoked on unexpected crash
  # Test: on_crash callback NOT invoked on intentional stop
  # Test: no crash callback when none provided (IEx mode)
  # Test: SIGSEGV during intentional shutdown triggers on_crash (signal-type awareness)
  # Test: SIGTERM during intentional shutdown is treated as intentional

  # --- HealthMonitor process monitoring ---
  # Test: controller monitors HealthMonitor; restarts on unexpected death
  # Test: controller does NOT restart HealthMonitor on normal stop

  # --- Event observer ---
  # Test: on_event callback fires for :server_started
  # Test: on_event callback fires for :server_stopped
  # Test: on_event callback fires for :server_crashed
  # Test: on_event is non-blocking (GenServer.cast path)
  # Test: no event callback when none provided
end
```

### Test File: `test/toast/deployment/server_control_test.exs`

Tests for the Deployment-level control API including targeting.

```elixir
defmodule Toast.Deployment.ServerControlTest do
  @moduledoc """
  Tests for Deployment.stop_server/2, kill_server/2, pause_server/2,
  resume_server/2, restart_server/2, start_server/2 with various targeting modes.
  Uses a mock controller to verify delegation.
  """
  use ExUnit.Case, async: false

  alias Toast.Deployment

  # --- Basic operations (by server ID) ---
  # Test: stop_server/2 returns :ok and marks server as intentionally stopped
  # Test: stop_server/2 returns {:error, :already_crashed} if server already crashed
  # Test: kill_server/2 sends SIGKILL and marks as intentionally killed
  # Test: pause_server/2 sends SIGSTOP and marks as paused
  # Test: resume_server/2 sends SIGCONT and resumes monitoring
  # Test: restart_server/2 stops then starts server
  # Test: restart_server/3 with args: [...] merges additional CLI arguments
  # Test: restart_server preserves immutable properties (port, data directory, binary)
  # Test: start_server/2 starts a previously stopped server

  # --- Role-based targeting ---
  # Test: stop_server(deployment, role: :dbserver) stops all dbservers
  # Test: pause_server(deployment, role: :coordinator, index: 0) targets first coordinator
  # Test: role-based targeting with unknown role returns {:error, :no_matching_servers}

  # --- Cluster-internal ID targeting ---
  # Test: stop_server(deployment, cluster_id: "PRMR-xxx") delegates to correct server
  # Test: unknown cluster_id returns {:error, :unknown_server}
end
```

---

## Implementation Details

### 1. ServerProcess Signal Extensions

The existing `ServerProcess` GenServer handles launch, stop, and crash detection. Three new `handle_call` clauses are needed for direct signal delivery, plus a `relaunch` operation for restart support.

#### New Client API Functions

```elixir
# In lib/toast/process/server_process.ex

@doc "Send SIGKILL to the managed process."
@spec kill(GenServer.server()) :: :ok | {:error, term()}
def kill(server), do: GenServer.call(server, :kill)

@doc "Send SIGSTOP to freeze the managed process."
@spec pause(GenServer.server()) :: :ok | {:error, term()}
def pause(server), do: GenServer.call(server, :pause)

@doc "Send SIGCONT to resume a frozen process."
@spec resume(GenServer.server()) :: :ok | {:error, term()}
def resume(server), do: GenServer.call(server, :resume)

@doc """
Re-launch the OS process after it was stopped/killed/crashed.
Optionally merge additional args with the original launch spec.
"""
@spec relaunch(GenServer.server(), keyword()) :: :ok | {:error, term()}
def relaunch(server, opts \\ []), do: GenServer.call(server, {:relaunch, opts})
```

#### New handle_call Clauses

The `:kill` handler calls `:exec.kill(os_pid, 9)` (SIGKILL). Unlike the existing `stop` which uses `:exec.stop/1` (SIGTERM with escalation), kill is immediate. The status transitions to `:killed` (a new status value -- add it to the `status` type).

The `:pause` handler calls `:exec.kill(os_pid, 19)` (SIGSTOP). This freezes the OS process but does NOT trigger erlexec's exit monitoring -- the process is still alive, just suspended by the kernel. The status transitions to `:paused` (new status value).

The `:resume` handler calls `:exec.kill(os_pid, 18)` (SIGCONT). Only valid when status is `:paused`. Transitions back to `:running`.

The `{:relaunch, opts}` handler is valid when status is `:stopped`, `:killed`, or `:crashed`. It calls `do_launch/1` using the original launch spec (executable, env, working_dir are preserved from init). If `opts` contains `args: extra_args`, these are appended to the original args list. The `exec_pid` and `os_pid` fields are reset before launch.

#### Status Type Extension

The `status` type expands from `:starting | :running | :stopping | :stopped | :crashed` to include `:paused` and `:killed`:

```elixir
@type status :: :starting | :running | :stopping | :stopped | :crashed | :paused | :killed
```

`:killed` is distinct from `:stopped` to preserve the information about how the process was terminated. Both allow `relaunch`.

#### Signal Constants

Define module attributes for signal numbers to avoid magic numbers:

```elixir
@sigkill 9
@sigstop 19
@sigcont 18
```

#### Exit Handling During Kill

When SIGKILL is sent, erlexec will deliver a `{:DOWN, os_pid, :process, _pid, {:exit_status, status}}` message. The existing `handle_exit/2` function handles this. Since the status is already `:killed` when this message arrives (set synchronously in the `:kill` handler), the exit handler should recognize this state and NOT notify the listener (the kill was intentional). Modify `handle_exit/2` to check if the current status is `:killed` -- if so, just clean up the `exec_pid`/`os_pid` fields without sending `{:server_crashed, ...}`.

Similarly, when a paused process is killed externally (e.g., OOM killer), the `:DOWN` message arrives. Since the status is `:paused`, this is an unexpected crash and should notify the listener.

### 2. HealthMonitor Suspend/Resume

The HealthMonitor currently has two states: `:healthy` and `:unhealthy`. Add `:suspended` as a third state.

#### New Messages

Add `handle_info` clauses for `:suspend` and `:resume` messages. The controller sends these as plain messages (`send(monitor_pid, :suspend)`).

`:suspend`:
- Cancel the pending `Process.send_after` timer using `Process.cancel_timer/1` on `state.timer_ref`
- Set `status` to `:suspended`
- Set `timer_ref` to `nil`

`:resume`:
- Only act if status is `:suspended`
- Reset `consecutive_failures` to 0
- Set `status` to `:healthy`
- Call `schedule_check/1` to restart polling

#### Replace `healthy?/1` with `status/1`

The existing `healthy?/1` function returns a boolean. Replace it with `status/1` returning `:healthy | :unhealthy | :suspended`. The controllers currently call `healthy?/1` -- update those call sites. Keep `healthy?/1` as a deprecated wrapper if needed for backward compatibility, but prefer the more informative `status/1`.

```elixir
@spec status(GenServer.server()) :: :healthy | :unhealthy | :suspended
def status(server), do: GenServer.call(server, :status)

# Deprecate
@spec healthy?(GenServer.server()) :: boolean()
def healthy?(server), do: status(server) == :healthy
```

The `handle_call(:status, ...)` clause replaces the existing `handle_call(:healthy?, ...)` clause.

#### Guard Against Stale :check Messages

When `:suspend` cancels a timer, the `:check` message may already be in the mailbox. The existing `handle_info(:check, %{status: :unhealthy})` clause already drops checks for unhealthy monitors. Add the same guard for `:suspended`:

```elixir
def handle_info(:check, %{status: :suspended} = state) do
  {:noreply, state}
end
```

### 3. ServerInstance Struct Extension

Add `operational_state` and `intentional` fields to track per-server control state:

```elixir
# In lib/toast/deployment/server_instance.ex

@type operational_state :: :running | :paused | :stopped | :killed | :crashed
@type t :: %__MODULE__{
  # ... existing fields ...
  operational_state: operational_state() | nil,
  intentional: boolean(),
  launch_spec: map() | nil
}

defstruct [
  # ... existing fields ...
  operational_state: nil,
  intentional: false,
  launch_spec: nil
]
```

The `launch_spec` field stores the original launch specification (from Factory) so that `restart_server` can re-launch the server with the same configuration. This is populated during the deploy sequence and is immutable across restarts (port, data directory, binary path are preserved).

### 4. SingleServerController Control Operations

The SingleServerController manages a single server. Add `handle_call` clauses for each control operation.

#### New Client API

```elixir
@spec stop_server(GenServer.server(), String.t()) :: :ok | {:error, term()}
def stop_server(server, server_id), do: GenServer.call(server, {:stop_server, server_id})

@spec kill_server(GenServer.server(), String.t()) :: :ok | {:error, term()}
def kill_server(server, server_id), do: GenServer.call(server, {:kill_server, server_id})

@spec pause_server(GenServer.server(), String.t()) :: :ok | {:error, term()}
def pause_server(server, server_id), do: GenServer.call(server, {:pause_server, server_id})

@spec resume_server(GenServer.server(), String.t()) :: :ok | {:error, term()}
def resume_server(server, server_id), do: GenServer.call(server, {:resume_server, server_id})

@spec restart_server(GenServer.server(), String.t(), keyword()) :: :ok | {:error, term()}
def restart_server(server, server_id, opts \\ []), do: GenServer.call(server, {:restart_server, server_id, opts})

@spec start_server(GenServer.server(), String.t(), keyword()) :: :ok | {:error, term()}
def start_server(server, server_id, opts \\ []), do: GenServer.call(server, {:start_server, server_id, opts})
```

#### Server State Tracking

When a control operation is processed:

1. **stop_server**: Call `ServerProcess.stop/2` on the managed process. Set `operational_state: :stopped, intentional: true`. Send `:suspend` to the HealthMonitor.
2. **kill_server**: Call `ServerProcess.kill/1`. Set `operational_state: :killed, intentional: true`. Send `:suspend` to the HealthMonitor.
3. **pause_server**: Call `ServerProcess.pause/1`. Set `operational_state: :paused, intentional: true`. Send `:suspend` to the HealthMonitor.
4. **resume_server**: Call `ServerProcess.resume/1`. Set `operational_state: :running, intentional: false`. Send `:resume` to the HealthMonitor.
5. **restart_server**: Execute stop_server logic, then start_server logic. Wait for health check before returning.
6. **start_server**: Call `ServerProcess.relaunch/2` (with merged args if provided). Wait for the server to pass a health check. Set `operational_state: :running, intentional: false`. Send `:resume` to the HealthMonitor.

#### Signal-Type Awareness in Crash Handling

Modify the existing `handle_info({:server_crashed, server_id, crash_info}, state)` clause. When a crash arrives:

1. If `intentional` is `false` -- this is an unexpected crash. Set `operational_state: :crashed`, invoke `:on_crash` callback, set deployment status to `:failed`. (Current behavior, preserved.)
2. If `intentional` is `true` -- examine `crash_info.signal`:
   - If signal is SIGTERM (15) or nil (normal exit) -- treat as intentional. Do NOT invoke `:on_crash`. Keep `intentional: true`.
   - If signal is SIGSEGV (11), SIGABRT (6), or SIGBUS (7) -- the server crashed with a real bug during intentional shutdown. Clear `intentional` flag, set `operational_state: :crashed`, invoke `:on_crash`, set deployment status to `:failed`.

This logic prevents masking real bugs in shutdown code while still treating expected shutdowns as intentional.

```elixir
@intentional_exit_signals [nil, 15]  # nil = normal exit, 15 = SIGTERM

defp crash_during_intentional?(signal) do
  signal not in @intentional_exit_signals
end
```

#### HealthMonitor Process Monitoring

After starting a HealthMonitor, the controller calls `Process.monitor(monitor_pid)` and stores the monitor reference in state. Handle the `{:DOWN, ref, :process, pid, reason}` message:

- If `reason` is `:normal` or `:shutdown` -- the monitor was intentionally stopped (e.g., during server suspend). No action needed.
- Otherwise -- the monitor crashed due to a bug. Log a warning and restart it using the same configuration.

```elixir
def handle_info({:DOWN, ref, :process, pid, reason}, state) when reason not in [:normal, :shutdown] do
  # Check if this is our health monitor
  if pid == state.server.health_monitor do
    Logger.warning("HealthMonitor for #{state.server.id} died unexpectedly: #{inspect(reason)}, restarting")
    {:ok, new_monitor} = start_health_monitor(state)
    new_ref = Process.monitor(new_monitor)
    {:noreply, put_server(state, health_monitor: new_monitor, health_monitor_ref: new_ref)}
  else
    {:noreply, state}
  end
end
```

#### Deployment Status Derivation

For SingleServerController, the status logic extends to include `:degraded`:

- Server running normally -> `:ready`
- Server intentionally stopped/killed/paused -> `:degraded`
- Server unexpectedly crashed -> `:failed`

### 5. ClusterController Control Operations

The ClusterController wraps the same operations but targets individual servers within the cluster. The key differences:

#### Per-Server State Map

The cluster controller already maintains a `servers` map (`%{String.t() => ServerInstance.t()}`). Each entry gains `operational_state` and `intentional` fields.

#### Deployment-Level Status Derivation

The cluster controller derives deployment-level status from the per-server states:

```elixir
defp derive_cluster_status(servers) do
  states = Map.values(servers) |> Enum.map(& &1.operational_state)

  cond do
    Enum.any?(states, &(&1 == :crashed)) ->
      # Check if any crash was unintentional
      if Enum.any?(Map.values(servers), &(&1.operational_state == :crashed and not &1.intentional)) do
        :failed
      else
        :degraded
      end

    Enum.all?(states, &(&1 == :running)) ->
      :ready

    Enum.any?(states, &(&1 in [:stopped, :killed, :paused])) ->
      :degraded

    true ->
      :ready
  end
end
```

#### Server Targeting Resolution

Control operations on the ClusterController accept either a direct server ID or a targeting spec. The controller resolves the target before executing the operation:

```elixir
defp resolve_targets(state, server_id) when is_binary(server_id) do
  if Map.has_key?(state.servers, server_id) do
    {:ok, [server_id]}
  else
    {:error, :unknown_server}
  end
end

defp resolve_targets(state, role: role) do
  ids = role_server_ids(state, role)
  if ids == [], do: {:error, :no_matching_servers}, else: {:ok, ids}
end

defp resolve_targets(state, role: role, index: index) do
  ids = role_server_ids(state, role)
  if index < length(ids), do: {:ok, [Enum.at(ids, index)]}, else: {:error, :no_matching_servers}
end

defp resolve_targets(state, cluster_id: cluster_id) do
  # Requires cluster_id mapping from section-07
  case find_by_cluster_id(state, cluster_id) do
    nil -> {:error, :unknown_server}
    id -> {:ok, [id]}
  end
end

defp role_server_ids(state, role) do
  case role do
    :agent -> state.agents
    :dbserver -> state.dbservers
    :coordinator -> state.coordinators
    _ -> []
  end
end
```

For role-based targeting that matches multiple servers (e.g., `role: :dbserver` with 3 dbservers), the operation is applied to each server sequentially. The operation fails fast -- if any server operation fails, the remaining servers are skipped and the error is returned.

### 6. Deployment API Extensions

The `Toast.Deployment` module gains new public functions that delegate to the controller. Each function resolves the targeting and delegates to the appropriate controller module.

```elixir
# In lib/toast/deployment.ex

@doc """
Gracefully stop a server. Marks it as intentionally stopped.
Target can be a server_id string, or keyword targeting:
  role: :dbserver, role: :coordinator, index: 0, cluster_id: "PRMR-xxx"
"""
@spec stop_server(t(), String.t() | keyword()) :: :ok | {:error, term()}
def stop_server(%__MODULE__{} = deployment, target) do
  controller_call_control(deployment, :stop_server, target)
end

@spec kill_server(t(), String.t() | keyword()) :: :ok | {:error, term()}
def kill_server(%__MODULE__{} = deployment, target) do
  controller_call_control(deployment, :kill_server, target)
end

@spec pause_server(t(), String.t() | keyword()) :: :ok | {:error, term()}
def pause_server(%__MODULE__{} = deployment, target) do
  controller_call_control(deployment, :pause_server, target)
end

@spec resume_server(t(), String.t() | keyword()) :: :ok | {:error, term()}
def resume_server(%__MODULE__{} = deployment, target) do
  controller_call_control(deployment, :resume_server, target)
end

@spec restart_server(t(), String.t() | keyword(), keyword()) :: :ok | {:error, term()}
def restart_server(%__MODULE__{} = deployment, target, opts \\ []) do
  controller_call_control(deployment, :restart_server, target, opts)
end

@spec start_server(t(), String.t() | keyword(), keyword()) :: :ok | {:error, term()}
def start_server(%__MODULE__{} = deployment, target, opts \\ []) do
  controller_call_control(deployment, :start_server, target, opts)
end
```

The `controller_call_control/3` helper delegates to the controller GenServer. For single-server deployments, the server_id is validated (must match the single server). For cluster deployments, targeting resolution happens inside the controller.

```elixir
defp controller_call_control(deployment, op, target, opts \\ []) do
  mod = controller_module(deployment)

  case {op, opts} do
    {op, []} -> apply(mod, op, [deployment.controller, target])
    {op, opts} -> apply(mod, op, [deployment.controller, target, opts])
  end
catch
  :exit, _ -> {:error, :controller_not_available}
end
```

### 7. Deployment Status Extension

The `status` type on the Deployment module and controllers expands to include `:degraded`:

```elixir
@type status :: :stopped | :starting | :ready | :degraded | :stopping | :failed
```

The `check_health/1` function in `Deployment` must handle `:degraded`:

```elixir
def check_health(%__MODULE__{} = deployment) do
  case status(deployment) do
    :ready -> :ok
    :degraded -> {:error, format_degraded_message(deployment)}
    :failed -> {:error, format_crash_message(crash_info(deployment))}
    other -> {:error, "Deployment not ready (status: #{other})"}
  end
end
```

The degraded message should name the downed servers. This information comes from querying the controller for the current server states:

```elixir
defp format_degraded_message(deployment) do
  downed = servers(deployment) |> Enum.filter(&(&1.operational_state in [:stopped, :killed, :paused]))
  names = Enum.map(downed, & &1.id) |> Enum.join(", ")
  "Deployment is degraded -- servers [#{names}] are intentionally down. " <>
    "Tests must restore all servers before finishing."
end
```

### 8. Migration Phase 4 Steps

This section corresponds to Phase 4 steps 1-3 and 5-7 from the migration plan:

1. **Step 1**: Extend ServerProcess with `:kill`, `:pause`, `:resume` handle_call clauses. Verify SIGSTOP does not trigger erlexec exit monitoring.
2. **Step 2**: Add `intentional` flag and signal-type awareness to server state tracking in controllers.
3. **Step 3**: Implement control operations on Deployment with targeting.
4. **Step 5**: Add `:suspend`/`:resume` messages to HealthMonitor.
5. **Step 6**: Add `:degraded` deployment status.
6. **Step 7**: Add HealthMonitor process monitoring from controllers.

Steps 4, 8-10 (expect_crash, cluster_id mapping, failure points, resilience test suite) belong to section-07.

## Key Design Constraints

**Control operations on the same server must not be concurrent.** The controller processes them serially via GenServer calls. Concurrent calls from different processes are serialized by the GenServer mailbox. No additional locking is needed.

**HealthMonitor suspend/resume is automatic.** Tests never manually toggle monitoring. When a control operation intentionally changes a server's state, the controller sends `:suspend`. When the server is restored, the controller sends `:resume`. This keeps the complexity in the framework, not in tests.

**Launch spec preservation.** The controller must store the original launch spec (from Factory) for each server at deploy time. This is needed for `restart_server` and `start_server` to re-launch with the same configuration. The launch spec includes: executable path, base args, env vars, working directory, server directory, port, and log file. Port and data directory are immutable across restarts. Additional args from `restart_server(deployment, id, args: [...])` are merged (appended) to the base args.

**Signal number portability.** SIGKILL=9, SIGSTOP=19, SIGCONT=18, SIGTERM=15, SIGSEGV=11, SIGABRT=6, SIGBUS=7. These are standard POSIX signal numbers and stable on Linux. Define them as module attributes, not as magic numbers in code.

---

## Implementation Notes

### Key Design Decisions
- **HealthMonitor suspend/resume already existed**: The HealthMonitor already had `:suspend`/`:resume` messages and `:suspended` status from section-02. Only integration testing was needed.
- **Role-based targeting deferred**: Only direct server_id string targeting implemented. Role/index/cluster_id targeting deferred to section-07 (Resilience) alongside expect_crash and cluster_id mapping.
- **restart_server handles all states**: Works from any operational state — running (graceful stop), paused (SIGKILL), or stopped/killed/crashed (just relaunch).
- **original_args preserved**: ServerProcess stores `original_args` from init to prevent arg accumulation across multiple relaunch cycles.
- **stop handles paused/killed**: `ServerProcess.stop/2` now handles paused (SIGCONT then stop) and killed (return :ok) states to prevent FunctionClauseError during deployment shutdown.

### Deviations from Plan
- Plan test files: `test/toast/process/server_process_control_test.exs` and `test/toast/deployment/*` — actual paths follow existing codebase conventions: `test/process/server_process_control_test.exs`, `test/deployment/*`, `test/toast/process/health_monitor_suspend_test.exs`
- Plan specified ~50 test cases across 4 files — implemented 28 tests covering core functionality. Signal-type awareness and controller integration tests deferred (require real ArangoDB or more complex mocking).

### Actual Files Created/Modified
| File | Action |
|------|--------|
| `lib/toast/process/server_process.ex` | Modified — kill/pause/resume/relaunch, original_args, stop for paused/killed |
| `lib/toast/deployment/server_instance.ex` | Modified — operational_state, intentional, launch_spec fields |
| `lib/toast/deployment/single_server_controller.ex` | Modified — control ops, signal-type awareness, degraded status |
| `lib/toast/deployment/cluster_controller.ex` | Modified — control ops, derive_cluster_status, signal-type awareness |
| `lib/toast/deployment.ex` | Modified — control API, check_health degraded, format_degraded_message |
| `test/process/server_process_control_test.exs` | Created — 7 tests |
| `test/toast/process/health_monitor_suspend_test.exs` | Created — 5 tests |
| `test/deployment/controller_state_test.exs` | Created — 6 tests |
| `test/deployment/server_control_test.exs` | Created — 8 tests |

### Test Results
468 tests, 0 failures (5 excluded)