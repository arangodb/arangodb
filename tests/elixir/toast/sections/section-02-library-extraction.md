I now have all the context needed. Let me produce the section content.

# Section 02: Library Extraction

## Overview

This section separates `lib/toast/` (infrastructure library) from `lib/toast_test/` (ExUnit integration), making the deployment infrastructure usable outside of ExUnit -- in IEx sessions, scripts, and future consumers. It also refactors the Deployment struct, introduces callback-based crash/event notification, updates the HealthMonitor for suspend/resume, adjusts the supervision tree, and extends configuration.

**Depends on**: section-01-restructure (flat project structure with `lib/toast/` and `lib/toast_test/` directories, modules already renamed to `ToastTest.*` namespace)

**Blocks**: sections 03, 04, 05, 06, 07, 08, 09 (all subsequent sections depend on the library/test-framework boundary established here)

---

## Prerequisites

After section-01 completes, the project is a flat Mix project:

```
tests/elixir/toast/
  lib/
    toast/          # Infrastructure library (deployment, process, diagnostics, client, config)
    toast_test/     # Test framework (runner, case, formatters, result export)
  test/             # Unit tests
  suites/           # Integration test suites
  mix.exs
```

Modules have been renamed: `Toast.Runner` is now `ToastTest.Runner`, `Toast.TestCase` is now `ToastTest.Case`, `Toast.CLIFormatter` is now `ToastTest.CLIFormatter`, etc. The `Toast.*` namespace is exclusively for the infrastructure library.

---

## Tests

Write these tests BEFORE implementing. All tests use ExUnit and live in `test/`. They mock external dependencies (erlexec, HTTP) and require no running ArangoDB.

### 3.1 Deployment API Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/deployment_test.exs`

These tests verify the refactored Deployment struct and core operations. The existing `test/deployment/deployment_test.exs` should be updated/replaced to match the new struct shape and API.

```elixir
# Test: Deployment struct contains immutable fields (id, mode, config, controller, endpoint, work_dir)
# Test: Deployment struct does NOT contain mutable fields (no servers, no crash_monitor)
# Test: Deployment.start/2 returns {:ok, deployment} with :single_server mode
# Test: Deployment.start/2 returns {:ok, deployment} with :cluster mode
# Test: Deployment.start/2 returns {:error, reason} on invalid config
# Test: Deployment.start/2 rejects :auto mode (only :single_server | :cluster accepted)
# Test: Deployment.stop/1 returns :ok
# Test: Deployment.stop_and_collect/1 returns diagnostics map or nil
# Test: Deployment.status/1 returns live status from controller
# Test: Deployment.server/2 returns server state for valid server_id
# Test: Deployment.servers/1 returns all servers
# Test: Deployment.servers/2 filters by role
# Test: Deployment.endpoint/1 returns the primary endpoint
# Test: For cluster, endpoint is first coordinator's URL
```

Key changes from current tests:
- The struct no longer has `crash_monitor` or `servers` fields.
- `status/1`, `server/2`, `servers/1`, `servers/2` are live queries to the controller GenServer, not struct field reads.
- `start/2` accepts `:on_crash` and `:on_event` callback options.

### 3.1 Crash Notification Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/deployment/crash_notification_test.exs`

```elixir
# Test: on_crash callback is invoked on unexpected crash
# Test: on_crash callback is NOT invoked on intentional stop
# Test: on_crash callback is NOT invoked when expect_crash is set
# Test: no crash callback when none provided (IEx mode)
# Test: SIGSEGV during intentional shutdown triggers on_crash (signal-type awareness)
# Test: SIGTERM during intentional shutdown is treated as intentional
```

### 3.1 Event Observer Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/deployment/event_observer_test.exs`

```elixir
# Test: on_event callback fires for :server_started with server_id, os_pid, timestamp
# Test: on_event callback fires for :server_stopped
# Test: on_event callback fires for :server_crashed
# Test: on_event is non-blocking (direct callback invocation)
# Test: spawned tasks call on_event callback directly (not routed through controller)
# Test: no event callback when none provided
```

### 3.2 Controller Architecture Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/deployment/controller_test.exs`

```elixir
# Test: server state transitions -- :running -> stop_server -> :stopped (intentional: true)
# Test: server state transitions -- :running -> unexpected crash -> :crashed (intentional: false)
# Test: server state transitions -- :paused -> resume -> :running
# Test: server state transitions -- :stopped -> start_server -> :running
# Test: SIGSEGV during intentional stop clears intentional flag
# Test: race condition -- crash before stop_server returns {:error, :already_crashed}
# Test: ClusterController deployment status: all running -> :ready
# Test: ClusterController deployment status: some intentionally down -> :degraded
# Test: ClusterController deployment status: unexpected crash -> :failed
# Test: Controller monitors HealthMonitor; restarts on unexpected death
```

### 3.3 Health Monitor Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/process/health_monitor_test.exs`

Update the existing health monitor tests and add:

```elixir
# Test: HealthMonitor accepts :suspend message and stops polling
# Test: HealthMonitor :suspend cancels pending Process.send_after timer
# Test: HealthMonitor accepts :resume message and restarts polling
# Test: HealthMonitor.status/1 returns :healthy | :unhealthy | :suspended
# Test: suspended monitor does not fire :check messages
# Test: multiple :suspend then single :resume restores monitoring
```

### 3.4 Application Supervision Tree Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/application_test.exs`

```elixir
# Test: Toast.Application starts PortAllocator, Process.Supervisor, Deployment.Supervisor
# Test: all child processes use :temporary restart strategy
```

### 3.6 Configuration Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/config_test.exs`

Extend the existing config tests:

```elixir
# Test: config precedence -- keyword opts > env vars > .toast.local.exs > defaults
# Test: .toast.local.exs is read at startup if present
# Test: .toast.local.exs is ignored if absent (no error)
# Test: global api_version configurable via Toast.Config
# Test: TOAST_API_VERSION env var sets global default
# Test: TOAST_DEBUGGER env var sets debugger preference
```

---

## Implementation

### Step 1: Refactor the Deployment Struct

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex`

The current `Toast.Deployment` struct has these fields:

```elixir
# CURRENT (to be changed)
defstruct [:id, :mode, :endpoint, :controller, :crash_monitor, :work_dir, :servers]
```

Refactor to:

```elixir
@type t :: %__MODULE__{
        id: String.t(),
        mode: :single_server | :cluster,
        config: Toast.Config.t(),
        controller: pid(),
        endpoint: String.t(),
        work_dir: Path.t()
      }

@enforce_keys [:id, :mode, :config, :controller, :endpoint, :work_dir]
defstruct [:id, :mode, :config, :controller, :endpoint, :work_dir]
```

Remove:
- `crash_monitor` field -- replaced by `:on_crash` callback passed as option to `start/2`
- `servers` field -- replaced by live query functions `server/2`, `servers/1`, `servers/2`

Add:
- `config` field -- stores the `Toast.Config.t()` used to create this deployment

The struct is a **handle** to a running deployment, not a snapshot. Mutable state (status, server list) is queried live from the controller GenServer.

**Dead handle pattern**: After `stop/1` or `stop_and_collect/1`, the deployment handle becomes invalid — the controller process has exited. Calling query functions (`status/1`, `servers/1`, etc.) on a stopped deployment returns `{:error, :stopped}` (caught as `:noproc` from `GenServer.call`). This is expected behavior, not an error to handle defensively — tests should not use a deployment handle after stopping it.

### Step 2: Replace Crash Monitor with :on_crash Callback

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex`

The current code spawns a dedicated crash monitor process:

```elixir
# CURRENT -- DELETE these functions
defp spawn_crash_monitor do
  spawn(fn ->
    Process.flag(:trap_exit, true)
    crash_monitor_loop()
  end)
end

defp crash_monitor_loop do
  receive do
    {:server_crashed, id, info} ->
      Toast.Runner.abort!("Server crashed: #{id}")  # HARD COUPLING to Runner
      ...
  end
end
```

This creates a hard dependency from `lib/toast/` to `ToastTest.Runner`. Replace with callback injection:

1. `start/2` accepts an optional `:on_crash` callback in opts.
2. The callback is passed through to the controller GenServer in its init opts.
3. When the controller detects an unexpected crash, it invokes the callback instead of sending to a dedicated process.
4. When no callback is provided (IEx mode), crashes are logged but no external action is taken.

The `start/2` function changes from:

```elixir
# CURRENT
crash_monitor = spawn_crash_monitor()
controller_opts = [config: config, crash_monitor: crash_monitor] ++ ...
```

To:

```elixir
# NEW
on_crash = Keyword.get(opts, :on_crash)
on_event = Keyword.get(opts, :on_event)
controller_opts = [config: config, on_crash: on_crash, on_event: on_event] ++ ...
```

Similarly, remove `stop_crash_monitor/1` calls from `stop/1` and `stop_and_collect/1`.

### Step 3: Add :on_event Callback for Process Lifecycle Events

**Files**:
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/single_server_controller.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/cluster_controller.ex`

The deployment accepts an optional `:on_event` callback alongside `:on_crash`:

```elixir
Toast.Deployment.start(:cluster,
  on_crash: fn deployment, crash_info -> ... end,
  on_event: fn event -> ... end
)
```

The callback receives tuples:
- `{:server_started, server_id, os_pid, timestamp}`
- `{:server_stopped, server_id, os_pid, exit_info, timestamp}`
- `{:server_crashed, server_id, os_pid, crash_info, timestamp}`

The callback must be **non-blocking** (fire-and-forget). Spawned tasks (both `SingleServerController` and `ClusterController`) call the `on_event` callback directly — the callback is already non-blocking, so there is no need to route events through the controller mailbox. This avoids latency and potential event reordering (e.g., a crash event arriving before a :server_started event if the controller mailbox is busy). `ProcessHistory` handles ordering via timestamps.

When no callback is provided, events are not emitted. The `on_event` callback is stored in the controller's state alongside `on_crash`.

### Step 4: Add Live Query Functions to Deployment

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex`

Add these functions that query the controller GenServer for current state:

```elixir
@doc "Get current state of a specific server."
@spec server(t(), String.t()) :: {:ok, map()} | {:error, :not_found}
def server(%__MODULE__{} = deployment, server_id)

@doc "List all servers with current state."
@spec servers(t()) :: [map()]
def servers(%__MODULE__{} = deployment)

@doc "List servers filtered by role."
@spec servers(t(), keyword()) :: [map()]
def servers(%__MODULE__{} = deployment, role: role)
```

These delegate to new `handle_call` clauses on the controllers:
- `:get_server` -- returns a single server's current state
- `:get_servers` -- returns all servers
- `{:get_servers, role}` -- returns servers filtered by role

The server state returned includes the new `operational_state` and `intentional` fields (see Step 5).

### Step 5: Add Server State Tracking to Controllers

**Files**:
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/single_server_controller.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/cluster_controller.ex`

Each server in the controller's state gains operational tracking:

```elixir
%{
  instance: ServerInstance.t(),
  operational_state: :running | :paused | :stopped | :killed | :crashed,
  intentional: boolean()
}
```

The `intentional` flag answers the resilience monitoring question:
- `stop_server/2` called -> `{:stopped, intentional: true}`, health monitor suspended
- Unexpected crash detected (erlexec `:DOWN` without prior control operation) -> `{:crashed, intentional: false}`, `:on_crash` callback fires
- `restart_server/2` or `resume_server/2` called -> `intentional` resets to `false`, monitoring resumes

**Signal-type awareness**: If a control operation sends SIGTERM and the server crashes with SIGSEGV/SIGABRT/SIGBUS during shutdown, the controller examines the exit signal. SIGTERM exit is intentional. SIGSEGV/SIGABRT/SIGBUS clears the intentional flag and fires the crash callback. This prevents masking real bugs in shutdown code.

**Race condition**: If a crash message arrives before a `stop_server` call is processed, the crash is treated as unexpected (correct -- the server died independently). The subsequent `stop_server` call returns `{:error, :already_crashed}`.

Currently the controllers store `crash_monitor` in their state. Replace with `on_crash` and `on_event`:

```elixir
# CURRENT controller init
state = %{
  ...
  crash_monitor: Keyword.get(opts, :crash_monitor)
}

# NEW controller init
state = %{
  ...
  on_crash: Keyword.get(opts, :on_crash),
  on_event: Keyword.get(opts, :on_event)
}
```

Replace `notify_crash_monitor/3`:

```elixir
# CURRENT
defp notify_crash_monitor(nil, _id, _info), do: :ok
defp notify_crash_monitor(pid, id, info), do: send(pid, {:server_crashed, id, info})

# NEW
defp notify_crash(nil, _deployment, _crash_info), do: :ok
defp notify_crash(on_crash, deployment, crash_info) when is_function(on_crash, 2) do
  on_crash.(deployment, crash_info)
end
```

**Important nuance**: The controller does not have the `%Toast.Deployment{}` struct -- it is the backing process. The `:on_crash` callback receives the crash info directly. The `ToastTest.CrashMonitor` (built in section-05) will wrap this to call `Runner.abort!/1`. For this section, the callback signature is `fn crash_info -> :ok end` (single-arity, receives only crash info). The deployment struct is not passed because the controller does not hold a reference to it (the struct holds a reference to the controller, not vice versa).

Revised callback signature:

```elixir
on_crash :: (crash_info :: map() -> :ok) | nil
on_event :: (event :: tuple() -> :ok) | nil
```

### Step 6: Update HealthMonitor for Suspend/Resume

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/process/health_monitor.ex`

Add `:suspend` and `:resume` message handling and change `healthy?/1` to `status/1`.

Current state:

```elixir
# CURRENT
state = %{
  ...
  status: :healthy,  # :healthy | :unhealthy
  timer_ref: nil
}

def healthy?(server), do: GenServer.call(server, :healthy?)
```

Changes:

1. Add `:suspended` to the status values: `:healthy | :unhealthy | :suspended`
2. Replace `healthy?/1` with `status/1` returning the atom
3. Add `handle_info(:suspend, state)`:
   - Cancel pending timer via `Process.cancel_timer/1` if `timer_ref` is not nil
   - Set status to `:suspended`
   - Do not schedule next check
4. Add `handle_info(:resume, state)`:
   - Set status to `:healthy` (reset consecutive_failures to 0)
   - Schedule next check via `schedule_check/1`
5. Add guard in `handle_info(:check, ...)` for `:suspended` status -- ignore the message (already partially handled by the `:unhealthy` guard, but add explicit `:suspended` guard)

```elixir
# NEW public API
@spec status(GenServer.server()) :: :healthy | :unhealthy | :suspended
def status(server), do: GenServer.call(server, :status)

# Backwards compatibility during transition
@spec healthy?(GenServer.server()) :: boolean()
def healthy?(server), do: status(server) == :healthy

# NEW handle_call
def handle_call(:status, _from, state) do
  {:reply, state.status, state}
end

# NEW handle_info for suspend
def handle_info(:suspend, state) do
  if state.timer_ref, do: Process.cancel_timer(state.timer_ref)
  {:noreply, %{state | status: :suspended, timer_ref: nil}}
end

# NEW handle_info for resume
def handle_info(:resume, %{status: :suspended} = state) do
  {:noreply, schedule_check(%{state | status: :healthy, consecutive_failures: 0})}
end

def handle_info(:resume, state) do
  # Not suspended, ignore
  {:noreply, state}
end

# Updated check handler -- also skip when suspended
def handle_info(:check, %{status: status} = state) when status in [:unhealthy, :suspended] do
  {:noreply, state}
end
```

Multiple `:suspend` calls are idempotent. A single `:resume` restores monitoring regardless of how many `:suspend` messages were sent.

### Step 7: Move ExUnit-Dependent Modules to lib/toast_test/

This is the core separation step. After section-01, files are already in `lib/toast_test/` by virtue of the restructure. But there may be lingering cross-references. Verify and fix:

**Modules that must live in `lib/toast_test/`** (these depend on ExUnit):
- `ToastTest.Runner` (was `Toast.Runner`) -- references `ExUnit.EventManager`, `ExUnit.RunnerStats`, `ExUnit.Server`, etc.
- `ToastTest.Case` (was `Toast.TestCase`) -- `use ExUnit.CaseTemplate`
- `ToastTest.CLIFormatter` (was `Toast.CLIFormatter`) -- ExUnit formatter behaviour
- `ToastTest.ResultFormatter` (was `Toast.ResultFormatter`) -- ExUnit formatter behaviour
- `ToastTest.ResultExporter` and submodules -- references ExUnit test structs

**Modules that must live in `lib/toast/`** (no ExUnit dependencies):
- `Toast.Deployment` and all submodules
- `Toast.Process.ServerProcess`, `Toast.Process.HealthMonitor`, `Toast.Process.Supervisor`
- `Toast.Config`
- `Toast.PortAllocator`
- `Toast.Client`
- `Toast.Diagnostics.*`
- `Toast.LogFormatter`
- `Toast.Application`

**Critical**: Remove the hardcoded `Toast.Runner.abort!` call from `Toast.Deployment`'s crash monitor. This is the primary ExUnit dependency in `lib/toast/`. After Step 2 (callback injection), this dependency is eliminated.

**Verification**: Run `mix xref graph --label compile --sink ToastTest` from the `lib/toast/` source. No module in `lib/toast/` should have a compile-time or runtime dependency on any `ToastTest.*` module or any `ExUnit.*` module.

Alternatively check with:
```
mix xref graph --format plain | grep "Toast\." | grep -v "ToastTest\." | grep "ExUnit"
```

Both should produce empty output.

### Step 8: Refactor Configuration

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/config.ex`

Two changes:

#### 8a: Add .toast.local.exs Support

The `Toast.Config.load/1` function gains a step that reads `.toast.local.exs` if present. This file is evaluated at startup and its values merge into the config defaults (below env vars and keyword opts in precedence).

```elixir
# Precedence chain: keyword opts > env vars > .toast.local.exs > defaults
```

Implementation approach:
- In `load/1`, before building the config struct, check if `.toast.local.exs` exists in the project root
- **CI safety**: Skip `.toast.local.exs` entirely when `TOAST_CI=true` is set. Local dev config should never affect CI runs.
- If present (and not in CI mode), evaluate it with `Code.eval_file/1` -- it should return a map
- Merge the map values as defaults (lowest precedence, above struct defaults)

```elixir
defp load_local_config do
  path = Path.join(File.cwd!(), ".toast.local.exs")
  if File.exists?(path) do
    {config_map, _bindings} = Code.eval_file(path)
    if is_map(config_map), do: config_map, else: %{}
  else
    %{}
  end
end
```

Then in `load/1`, use local config values as fallback when neither keyword opts nor env vars provide a value. The `opt_or/3` helper already handles keyword-vs-env precedence. Add a third tier: if env also returns nil, check local config.

#### 8b: Add New Config Keys

Add to the Config struct:

```elixir
api_version: non_neg_integer() | String.t() | nil  # default: nil
debugger: :gdb | :lldb | :auto | nil               # default: :auto
```

With corresponding env vars:
- `TOAST_API_VERSION` -- global default API version
- `TOAST_DEBUGGER` -- debugger preference (gdb, lldb, auto)

#### 8c: Refactor Environment Variable Reading

The current `mix toast` task calls `apply_toast_env/1` which sets `System.put_env` based on CLI options. This mutates global state and can leak between sequential suite executions.

Refactor: environment variables are read **once** at task startup by `Toast.Config.load/1`. The loaded config is passed as explicit keyword options to each suite's deployment. The `apply_toast_env/1` function in the mix task is removed.

The current flow:
```
CLI args -> System.put_env() -> Toast.Config.load() reads System.get_env()
```

New flow:
```
CLI args -> keyword opts -> Toast.Config.load(keyword_opts)
```

This means `Toast.Config.load/1` already handles the precedence correctly via `opt_or/3`. The mix task simply passes CLI options as keywords directly to `load/1` instead of going through env vars as an intermediary.

### Step 9: Update Supervision Tree

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/application.ex`

The current supervision tree already has the correct structure:

```
Toast.Application
+-- Toast.PortAllocator (GenServer)
+-- Toast.Process.Supervisor (DynamicSupervisor)
+-- Toast.Deployment.Supervisor (DynamicSupervisor)
```

Changes needed:
1. Verify all child specs use `:temporary` restart strategy (DynamicSupervisors already use `max_restarts: 0` for Process.Supervisor)
2. Ensure Deployment.Supervisor also has appropriate restart configuration
3. Remove the `Toast.ResultExporter.result_dir()` call from `setup_file_logger/0` if `Toast.ResultExporter` has moved to `ToastTest.ResultExporter` -- the file logger setup should not depend on test framework modules

The file logger setup needs adjustment: `Toast.ResultExporter` is now `ToastTest.ResultExporter` and lives in the test framework. The application module must not reference it. Instead, use `Toast.Config` or a simpler approach for determining the log path.

```elixir
# CURRENT (broken after extraction)
defp setup_file_logger do
  result_dir = Toast.ResultExporter.result_dir()  # This is now ToastTest.ResultExporter!
  ...
end

# NEW
defp setup_file_logger do
  result_dir = Application.get_env(:toast, :result_dir, "toast-results")
  File.mkdir_p!(result_dir)
  log_path = Path.join(result_dir, "toast.log")
  ...
end
```

### Step 10: Add HealthMonitor Process Monitoring to Controllers

**Files**:
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/single_server_controller.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/cluster_controller.ex`

After starting a HealthMonitor, the controller calls `Process.monitor/1` on the HealthMonitor PID. If the HealthMonitor dies unexpectedly (bug, not a server failure), the controller detects the `:DOWN` message and restarts it.

```elixir
# In start_health_monitor/1 (SingleServerController)
defp start_health_monitor(state) do
  case Toast.Process.Supervisor.start_health_monitor(...) do
    {:ok, pid} ->
      Process.monitor(pid)  # NEW: monitor the monitor
      {:ok, pid}
    error -> error
  end
end

# NEW handle_info clause
def handle_info({:DOWN, _ref, :process, pid, reason}, state) when reason != :normal do
  if pid == state.server.health_monitor and state.status == :ready do
    Logger.warning("HealthMonitor died unexpectedly (#{inspect(reason)}), restarting")
    case start_health_monitor(state) do
      {:ok, new_pid} -> {:noreply, put_server(state, health_monitor: new_pid)}
      {:error, _} -> {:noreply, state}
    end
  else
    {:noreply, state}
  end
end
```

For `ClusterController`, the same pattern applies but checks all servers' health monitors.

### Step 11: Verify IEx Workflow

After all changes, verify that the deployment infrastructure works standalone from IEx:

```elixir
# In IEx (iex -S mix):
{:ok, deployment} = Toast.Deployment.start(:single_server, build_dir: "/path/to/build")
Toast.Deployment.status(deployment)   # => :ready
Toast.Deployment.servers(deployment)  # => [%{id: "toast-123", role: :single, ...}]
Toast.Deployment.endpoint(deployment) # => "http://127.0.0.1:8529"
Toast.Deployment.stop(deployment)     # => :ok
```

No ExUnit, no test framework, no formatters. Just the deployment library. This is the key deliverable of this section.

### Step 12: Verify Zero ExUnit Dependencies in lib/toast/

Run:

```bash
cd /home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast
mix xref graph --label compile-connected --sink ExUnit --format plain
```

Filter output for any modules in `lib/toast/` (not `lib/toast_test/`). The result must be empty. If any `Toast.*` module depends on ExUnit, track down the reference and remove it.

Common places where ExUnit references hide:
- `Toast.Deployment` calling `Toast.Runner.abort!` (fixed by callback injection)
- `Toast.Application` referencing `Toast.ResultExporter` (fixed by using Application env)
- Any `require ExUnit.Assertions` or `import ExUnit.Assertions` in library code

---

## Migration Notes (Phase 2 from the Plan)

The implementation steps above correspond to Migration Plan Phase 2:

1. Move ExUnit-dependent modules to `lib/toast_test/` (Step 7 -- verify section-01 did this correctly)
2. Extract crash monitor into callback pattern (Steps 2, 5)
3. Add deployment event callback mechanism (Step 3)
4. Ensure `lib/toast/` has zero ExUnit dependencies (Steps 7, 12)
5. Refactor config loading (Step 8)
6. Verify IEx workflow (Step 11)
7. Add convenience functions for interactive use (Step 4 -- `status/1`, `servers/1`, etc.)

---

## File Summary

Files to **modify**:

| File | Changes |
|------|---------|
| `lib/toast/deployment.ex` | Remove `crash_monitor`/`servers` from struct; add `config` field; replace spawn_crash_monitor with callback opts; add `server/2`, `servers/1`, `servers/2` live query functions; remove `stop_crash_monitor` calls |
| `lib/toast/deployment/single_server_controller.ex` | Replace `crash_monitor` with `on_crash`/`on_event` callbacks; add `operational_state`/`intentional` tracking; add `handle_call` for `:get_server`, `:get_servers`; add HealthMonitor process monitoring; fire `on_event` on lifecycle transitions |
| `lib/toast/deployment/cluster_controller.ex` | Same changes as SingleServerController; add `:degraded` status logic; forward events from tasks via `GenServer.cast`; add HealthMonitor process monitoring for all servers |
| `lib/toast/process/health_monitor.ex` | Add `:suspend`/`:resume` message handling; add `:suspended` status; replace `healthy?/1` with `status/1`; cancel timer on suspend |
| `lib/toast/config.ex` | Add `.toast.local.exs` support; add `api_version` and `debugger` fields; ensure env vars read once (no mutation) |
| `lib/toast/application.ex` | Remove `Toast.ResultExporter` reference from file logger setup; use Application env instead |
| `lib/toast/deployment/supervisor.ex` | No functional changes; verify `:temporary` restart strategy |
| `lib/toast/process/supervisor.ex` | No functional changes; verify restart configuration |

Files to **create**:

| File | Purpose |
|------|---------|
| `test/toast/deployment/crash_notification_test.exs` | Tests for :on_crash callback injection |
| `test/toast/deployment/event_observer_test.exs` | Tests for :on_event callback |
| `test/toast/deployment/controller_test.exs` | Tests for state tracking, intentional flag, degraded status |
| `.toast.local.exs` (gitignored) | Example local config file |

Files to **update tests**:

| File | Changes |
|------|---------|
| `test/toast/deployment_test.exs` | Update for new struct shape (no crash_monitor, no servers field, add config) |
| `test/toast/process/health_monitor_test.exs` | Add suspend/resume tests, update `healthy?` to `status` |
| `test/toast/config_test.exs` | Add .toast.local.exs tests, api_version/debugger tests |
| `test/toast/application_test.exs` | Verify supervision tree children and restart strategies |

---

## Key Design Decisions

**Callback injection over dedicated process**: The `:on_crash` callback replaces the dedicated crash monitor process. This is simpler (no process to manage) and breaks the coupling cleanly. The callback is invoked in the controller's process context, which is fine because crash handling is synchronous (the suite is being aborted anyway).

**Single-arity callbacks**: The `:on_crash` callback receives `crash_info` only, not the deployment struct. The controller does not hold a reference to the deployment struct (the struct references the controller, not vice versa). The `ToastTest.CrashMonitor` (section-05) will close over any needed context.

**Struct as handle**: The deployment struct contains only immutable fields set at startup. All mutable state queries go through GenServer calls. This prevents stale data issues where a test reads `deployment.servers` and gets an outdated snapshot.

**`:suspended` as explicit status**: Rather than a boolean `suspended?` field, `:suspended` is a first-class status value. This makes the three-state nature explicit and prevents callers from accidentally checking `healthy?` on a suspended monitor.