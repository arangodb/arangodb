Now I have a thorough understanding of the codebase and the plan. Let me produce the section content.

# Section 4: Suite System

## Overview

This section implements the suite abstraction layer that replaces the current single-deployment-per-run model. Each suite declares its own deployment requirements, and the framework manages per-suite deployment lifecycle. The section covers:

- `ToastTest.Suite` behaviour and `use ToastTest.Suite` macro
- `ToastTest.Case` base test case template
- `ToastTest.ProcessHistory` observer for process lifecycle events
- Server ID mapping (`cluster_id/2`, `server_by_cluster_id/2`)
- `ToastTest.Interactive` for IEx-based test execution
- Inter-suite state cleanup
- Result export with suite-level grouping
- Suite discovery and CLI interface in `mix toast`

**Dependencies**: Sections 01 (restructured project) and 02 (library extraction with callback injection, `Toast.Deployment` API, supervision tree, configuration). This section assumes `lib/toast/` and `lib/toast_test/` already exist as separate namespaces with zero ExUnit coupling in `lib/toast/`.

**Blocks**: Section 05 (Runner Refactoring) depends on the suite system defined here.

---

## Tests First

All tests live under `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/`. These are unit tests for the framework and run with `mix test` (no ArangoDB needed). Where tests interact with deployments or controllers, use Mox or simple stubs.

### 4.1 Suite System Design Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/suite_test.exs`

```elixir
defmodule ToastTest.SuiteTest do
  use ExUnit.Case, async: true

  # Test: use ToastTest.Suite turns suite module into CaseTemplate (injects use ExUnit.CaseTemplate)
  # Test: test modules doing `use MySuite` get __toast_suite__/0 function returning MySuite
  # Test: @toast_suite attribute is set to the suite module
  # Test: deployment_config/0 callback returns keyword config
  # Test: mode: :auto is the default in deployment_config
  # Test: setup_deployment/1 optional callback — suite without it works fine (function_exported? check)
  # Test: setup_deployment/1 runs once per suite, not per test
  # Test: setup_deployment/1 returning {:error, reason} → tests marked :errored, deployment stopped
  # Test: setup_deployment/1 result merges into test context (override semantics — can override :client)
  # Test: server_args merged with global defaults
  # Test: coordinator_args applies only to coordinators
  # Test: dbserver_args applies only to dbservers
  # Test: agent_args applies only to agents
  # Test: role-specific args take precedence over server_args for that role
end
```

### Suite Discovery Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/suite_discovery_test.exs`

```elixir
defmodule ToastTest.SuiteDiscoveryTest do
  use ExUnit.Case, async: true

  # Test: ExUnit.start(autorun: false) called before any compilation
  # Test: mix toast discovers suite.ex files in suites/ subdirectories
  # Test: suite modules identified by ToastTest.Suite behaviour
  # Test: suite.ex compiled first (globally), then *.ex helpers (per-suite), then test_*.exs
  # Test: compilation constraint: suite modules must not depend on helpers in same folder
  # Test: ExUnit.Server modules_loaded/1 NOT called
  # Test: orphan .exs file detection (warn on non-test_*.exs files in suite folders)
end
```

### CLI Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/cli_test.exs`

```elixir
defmodule ToastTest.CLITest do
  use ExUnit.Case, async: true

  # Test: mix toast (no args) runs all suites
  # Test: mix toast smoke runs single suite
  # Test: mix toast smoke shell_server runs multiple suites (space-separated)
  # Test: mix toast smoke/test_version.exs runs specific file
  # Test: mix toast smoke/test_version.exs:42 runs specific line
  # Test: --cluster flag sets deployment mode to :cluster
  # Test: --single flag sets deployment mode to :single_server
  # Test: --test "pattern" filters by test name
  # Test: --no-agency-dump disables agency dump
end
```

### 4.4 Test Case Template Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/case_test.exs`

```elixir
defmodule ToastTest.CaseTest do
  use ExUnit.Case, async: true

  # Test: ToastTest.Case setup provides %{deployment: _, endpoint: _, client: _}
  # Test: deployment handle read from ETS registry keyed by suite module
  # Test: health check between tests rejects :degraded with clear error message naming downed servers
  # Test: health check between tests rejects :failed
  # Test: :ready status allows next test to proceed
end
```

### 4.5 Process History Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/process_history_test.exs`

```elixir
defmodule ToastTest.ProcessHistoryTest do
  use ExUnit.Case, async: true

  # Test: ProcessHistory records :server_started events keyed by OS PID
  # Test: ProcessHistory records :server_stopped events
  # Test: ProcessHistory records :server_crashed events
  # Test: events timestamped
  # Test: history used to correlate sanitizer log files (by PID) to server instances
  # Test: history cleared between suites
end
```

### 4.6 Server ID Mapping Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/server_id_mapping_test.exs`

```elixir
defmodule ToastTest.ServerIdMappingTest do
  use ExUnit.Case, async: true

  # Test: cluster_id/2 returns cluster-internal ID for toast ID
  # Test: server_by_cluster_id/2 returns server info for cluster-internal ID
  # Test: mapping fetched from /_admin/cluster/health after cluster formation
  # Test: mapping cached in ClusterController state
  # Test: mapping stable across server restarts (data dir preserved, same ID)
  # Test: control operations accept cluster_id: targeting
end
```

### 4.7 Interactive Test Execution Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/interactive_test.exs`

```elixir
defmodule ToastTest.InteractiveTest do
  use ExUnit.Case, async: true

  # Test: Interactive.run/2 with module atom runs all tests in module
  # Test: Interactive.run/2 with file path compiles file via Code.compile_file then runs
  # Test: Interactive.run/3 with test name runs single test
  # Test: recompilation replaces module in BEAM (fresh __ex_unit__ metadata)
  # Test: ExUnit.Server accumulation from recompilation is harmless
  # Test: results returned without deployment start/stop or result export
end
```

### 4.8 Inter-Suite State Cleanup Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/state_cleanup_test.exs`

```elixir
defmodule ToastTest.StateCleanupTest do
  use ExUnit.Case, async: true

  # Test: ETS deployment registry cleared
  # Test: ExUnit abort table (ETS) cleared
  # Test: ExUnit.after_suite callbacks cleared between suites
  # Test: formatter state reset (GenServers stopped and restarted)
  # Test: port allocator NOT reset (continues allocating to avoid TIME_WAIT conflicts)
  # Test: SuiteRun struct fields are NOT in Application.put_env (no global state to clean)
end
```

### 4.9 Result Export Tests

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/result_export_test.exs`

```elixir
defmodule ToastTest.ResultExportTest do
  use ExUnit.Case, async: true

  # Test: results include suite-level grouping
  # Test: global summary aggregates across suites
  # Test: per-suite and per-test timing included
  # Test: JUnit XML includes suite-level <testsuite> elements
end
```

---

## Implementation

### 4.1 Suite System Design

#### Behaviour Definition

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/suite.ex`

The `ToastTest.Suite` behaviour defines the contract that suite modules implement. It has one required callback and two optional callbacks:

```elixir
defmodule ToastTest.Suite do
  @moduledoc """
  Behaviour for Toast test suites.

  A suite declares deployment requirements and optionally provides
  setup/teardown logic that runs once per suite (not per test).
  """

  @callback deployment_config() :: keyword()
  @callback setup_deployment(Toast.Deployment.t()) :: {:ok, map()} | {:error, term()}
  @callback teardown_deployment(Toast.Deployment.t()) :: :ok
  @callback between_tests(Toast.Deployment.t(), ExUnit.Test.t()) :: :ok | {:error, term()}
  @optional_callbacks [setup_deployment: 1, teardown_deployment: 1, between_tests: 2]

  defmacro __using__(opts) do
    # ... macro body described below
  end
end
```

#### The `use ToastTest.Suite` Macro

The macro turns the calling module into an ExUnit CaseTemplate. It does the following:

1. Injects `use ExUnit.CaseTemplate` into the suite module.
2. Implements the `ToastTest.Suite` behaviour with a `deployment_config/0` that returns the opts passed to `use`.
3. Defines a `__using__` macro in the suite module. When a test module does `use MySuite`, the `__using__` macro:
   - Injects `use ToastTest.Case` to get the base test context.
   - Sets `@toast_suite` to the suite module.
   - Defines `def __toast_suite__`, returning the suite module.

The key insight is that `__MODULE__` inside the `__using__` macro definition is evaluated in the suite module's context (via `unquote(__MODULE__)`), so the attribute and function always resolve to the suite module, not the test module.

**Deployment config options** accepted by `use ToastTest.Suite`:

| Option | Default | Description |
|--------|---------|-------------|
| `mode` | `:auto` | `:auto`, `:single_server`, or `:cluster` |
| `timeout` | `3_600_000` (1h) | Suite timeout in milliseconds |
| `server_args` | `[]` | Extra CLI args for all arangod processes |
| `coordinator_args` | `[]` | Extra CLI args for coordinators only |
| `dbserver_args` | `[]` | Extra CLI args for dbservers only |
| `agent_args` | `[]` | Extra CLI args for agents only |
| `cluster_dbservers` | (global default) | Number of dbservers |
| `cluster_coordinators` | (global default) | Number of coordinators |
| `cluster_agents` | (global default) | Number of agents |
| `between_tests` | `:default` | `:default` (check `:ready`), `false` (skip), or `&callback/2` |

**`:auto` resolution**: The runner (Section 05) resolves `:auto` to an actual mode (`:single_server` or `:cluster`) before calling `Toast.Deployment.start/2`. Resolution uses the global mode from `Toast.Config` (set via CLI `--cluster`/`--single`, env var, or `.toast.local.exs`). The `Toast.Deployment.start/2` API only accepts `:single_server` or `:cluster`.

**Argument merging**: Role-specific args (e.g., `coordinator_args`) take precedence over `server_args` for that role. Both are merged with global defaults from `Toast.Config`. The merge order is: global defaults < suite `server_args` < suite role-specific args.

#### Example Suite Module

```elixir
# suites/smoke/suite.ex
defmodule Smoke.Suite do
  use ToastTest.Suite
end
```

A minimal suite with no custom configuration. It uses `mode: :auto`, the default timeout, and no extra arguments.

```elixir
# suites/shell_server/suite.ex
defmodule ShellServer.Suite do
  use ToastTest.Suite,
    timeout: 600_000,
    server_args: ["--javascript.enabled", "true"]
end
```

A suite that enables JavaScript on all servers and has a 10-minute timeout.

```elixir
# suites/resilience/suite.ex
defmodule Resilience.Suite do
  use ToastTest.Suite,
    mode: :cluster,
    cluster_dbservers: 3,
    cluster_coordinators: 2

  def setup_deployment(deployment) do
    # Custom cluster validation after deployment is healthy
    {:ok, %{}}
  end
end
```

A suite that requires a cluster and does one-time setup after deployment starts.

#### Example Test Module

```elixir
# suites/smoke/test_version.exs
defmodule Smoke.VersionTest do
  use Smoke.Suite

  test "returns arango server info", %{client: client} do
    assert {:ok, body} = Toast.Client.Admin.version(client)
    assert body["server"] == "arangox"
  end
end
```

When `use Smoke.Suite` is expanded, the test module gets:
- `use ToastTest.Case` (base context with `%{deployment, endpoint, client}`)
- `@toast_suite Smoke.Suite`
- `def __toast_suite__, do: Smoke.Suite`

#### Suite Discovery and File Conventions

Suites live in `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/`. Each subdirectory contains:

- `suite.ex` -- Suite definition module (exactly one per folder)
- `*.ex` -- Helper modules (compiled, not run as tests)
- `test_*.exs` -- Test files (loaded and run by the runner)

The `test_` prefix (rather than `_test.exs` suffix) groups test files together in directory listings and distinguishes them from helpers and `suite.ex`.

**Discovery algorithm** (in `mix toast` task):

1. Call `ExUnit.start(autorun: false)` before any suite compilation. This must happen before modules that `use ExUnit.CaseTemplate` are compiled, since ExUnit's application env must be initialized. The `autorun: false` flag prevents ExUnit from running tests on its own.
2. Find all `suites/*/suite.ex` files and compile them in a batch. Suite modules must not depend on helper modules in the same folder (they can only depend on `lib/` modules).
3. Identify suite modules by checking which loaded modules implement the `ToastTest.Suite` behaviour (via `behaviours = module_info(:attributes)[:behaviour] || []`).
4. For each requested suite (or all if none specified), sequentially:
   a. Compile `*.ex` helper files in the suite's folder.
   b. Load `test_*.exs` files in the suite's folder (via `Code.require_file/1` or `Kernel.ParallelCompiler.require/1`).
   c. Identify test modules that belong to this suite via `module.__toast_suite__() == suite_module`.
   d. Hand modules to the runner for execution (Section 05).

**ExUnit.Server bypass**: The runner does NOT call `ExUnit.Server.modules_loaded/1` or `ExUnit.Server.take_sync_modules/0` or `ExUnit.Server.take_async_modules/1`. Modules auto-register with `ExUnit.Server` during compilation (because they `use ExUnit.CaseTemplate`), but this is harmless dead state. The runner drives execution via `module.__ex_unit__()` directly.

**Orphan file detection**: After loading test files, the task scans each suite folder for `.exs` files that don't match the `test_*.exs` pattern. These get a warning: "Warning: suites/smoke/setup.exs is not a test file (must start with test_) and is not compiled as a helper (must end in .ex). This file is ignored."

**Module namespace requirement**: Suite-local helper modules (compiled from `*.ex` files) AND test modules (from `test_*.exs` files) MUST be namespaced under the suite module. For example, modules in `suites/smoke/` must be under `Smoke.*`. This prevents silent module replacement if two suites define a module with the same name (e.g., both defining `VersionTest` at the top level — the BEAM would silently replace the first). Add a compiler check: after compiling a suite's helpers and test files, verify all new modules are namespaced under the suite's root namespace.

**Suite compilation failure handling**: If a suite's `suite.ex` or helper files fail to compile, the error is logged and that suite is marked as errored (all its tests become `:errored` with the compilation error). Other suites continue normally. A compile error in one suite must not prevent other suites from running.

#### CLI Interface

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/mix/tasks/toast.ex`

The existing `Mix.Tasks.Toast` is refactored for path-based suite selection:

```
mix toast                                    # Run all suites
mix toast smoke                              # Run suites/smoke/
mix toast smoke shell_server                 # Run two suites (space-separated)
mix toast smoke/test_version.exs             # Run specific file in smoke suite
mix toast smoke/test_version.exs:42          # Run specific line
mix toast --cluster                          # All suites in cluster mode
mix toast smoke --single                     # Smoke suite in single-server mode
mix toast --test "version endpoint"          # Filter by test name pattern
mix toast --no-agency-dump                   # Disable agency dump
```

Arguments are paths relative to the `suites/` directory. A bare name like `smoke` means "all tests in `suites/smoke/`". A path with a filename like `smoke/test_version.exs` selects the smoke suite but only runs that file. The `:42` suffix filters to a specific line (using ExUnit's `only_test_ids` mechanism).

Mode flags `--cluster` and `--single` override the global deployment mode. They are resolved once at startup and passed as keyword opts to each suite's deployment — environment variables are NOT mutated between suites.

New switches to add beyond the existing ones:

| Switch | Type | Description |
|--------|------|-------------|
| `--test` | `:string` | Filter tests by name pattern |
| `--no-agency-dump` | `:boolean` | Disable agency dump for cluster deployments |

---

### 4.4 Test Case Template

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/case.ex`

The `ToastTest.Case` module replaces the existing `Toast.TestCase` as the base CaseTemplate. It provides three keys in the test context:

- `%{deployment: deployment}` -- The `Toast.Deployment` handle for the current suite's deployment
- `%{endpoint: endpoint}` -- The primary endpoint URL (coordinator for cluster, server for single)
- `%{client: client}` -- A `Toast.Client` struct for REST API calls (unauthenticated by default)

**Deployment handle delivery**: The runner stores the active deployment in an ETS-based registry (`ToastTest.DeploymentRegistry`) keyed by suite module before running a suite's tests. The `ToastTest.Case` `setup` callback reads the deployment from ETS using the test module's `__toast_suite__/0` to determine the key. This replaces `Application.put_env` to eliminate a concurrency hazard — ExUnit setup callbacks and `on_exit` callbacks run in spawned processes, and a stale `on_exit` from a previous test could theoretically race with the next suite's deployment being stored.

```elixir
defmodule ToastTest.Case do
  @moduledoc """
  Base CaseTemplate providing deployment context to tests.
  """

  use ExUnit.CaseTemplate

  setup context do
    suite_module = context.module.__toast_suite__()
    deployment = ToastTest.DeploymentRegistry.get(suite_module)
    client = Toast.Client.new(deployment.endpoint)
    %{deployment: deployment, endpoint: deployment.endpoint, client: client}
  end
end
```

The `ToastTest.DeploymentRegistry` is a thin wrapper around a named ETS table:

```elixir
defmodule ToastTest.DeploymentRegistry do
  @table :toast_deployment_registry

  def init, do: :ets.new(@table, [:named_table, :public, :set])
  def put(suite_module, deployment), do: :ets.insert(@table, {suite_module, deployment})
  def get(suite_module) do
    case :ets.lookup(@table, suite_module) do
      [{^suite_module, deployment}] -> deployment
      [] -> raise "No deployment registered for suite #{inspect(suite_module)}"
    end
  end
  def clear, do: :ets.delete_all_objects(@table)
end
```

The `using` macro in `ToastTest.Case` can also inject common aliases (e.g., `alias Toast.Client`).

**Health check between tests**: This is handled by the runner (Section 05), not by the case template. The check is configurable per suite via the `between_tests` option:
- **`:default`** (or omitted): The runner calls `Toast.Deployment.status/1`. Only `:ready` proceeds. `:failed` aborts the suite. `:degraded` aborts with a clear error naming downed servers.
- **`false`**: Skip the check entirely. Useful for fast suites that never manipulate servers.
- **`&callback/2`**: Custom callback `fn deployment, test -> :ok | {:error, reason} end`. The suite can implement the `between_tests/2` behaviour callback for this purpose.

This flexibility lets smoke suites skip the check (no server manipulation → no risk) while resilience suites can add extensive verification logic beyond a simple status check.

**Migration path**: The existing `Toast.TestCase` is preserved as a thin wrapper that delegates to `ToastTest.Case` and emits a compilation warning suggesting migration. This allows incremental migration of existing test files.

---

### 4.5 Process History *(Deferred to Phase 4 — Resilience)*

> **Note**: ProcessHistory is deferred to Phase 4 (section-07). It is only needed for resilience tests with server restarts, where diagnostics must be correlated to specific server instances across restarts. The existing CrashMatcher/SanitizerMatcher already use timestamps for basic correlation, which is sufficient for Phase 3b. The `:on_event` callback mechanism (from section-02) stays in Phase 2 — only the ProcessHistory consumer is deferred.

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/process_history.ex`

A GenServer that records process lifecycle events for diagnostics attribution. It is provided as the `:on_event` callback when the runner starts a suite's deployment:

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

The `handle_event/1` function calls `GenServer.cast` internally, ensuring non-blocking behavior. Neither the deployment controller nor the launch tasks are blocked by event processing.

The GenServer maintains a log of events keyed by OS PID and timestamped. This enables:
- Correlating sanitizer log files (named with OS PID) to specific server instances
- Correlating core dumps (named with OS PID) to server instances
- Determining which test was running when a server crashed

```elixir
defmodule ToastTest.ProcessHistory do
  @moduledoc """
  Records process lifecycle events for diagnostics attribution.
  """

  use GenServer

  ## Client API

  def start_link(opts \\ []) do
    GenServer.start_link(__MODULE__, %{}, opts)
  end

  @doc "Event callback for deployment :on_event option."
  def handle_event(event) do
    # Cast to the registered name; non-blocking
    GenServer.cast(__MODULE__, {:event, event})
  end

  @doc "Retrieve all recorded events."
  def events do
    GenServer.call(__MODULE__, :events)
  end

  @doc "Clear all recorded events (between suites)."
  def clear do
    GenServer.cast(__MODULE__, :clear)
  end

  ## Server callbacks
  # init/1, handle_cast/2 for {:event, event} and :clear, handle_call/2 for :events
end
```

The process is started as part of the test framework's supervision (registered as `ToastTest.ProcessHistory`). It is cleared between suites as part of inter-suite state cleanup (Section 4.8).

---

### 4.6 Server ID Mapping

This functionality lives in the `Toast.Deployment` API (library layer, `lib/toast/`) because it operates on deployment infrastructure. Two functions are exposed:

```elixir
Toast.Deployment.cluster_id(deployment, "dbserver-0")
# => "PRMR-abc123"

Toast.Deployment.server_by_cluster_id(deployment, "PRMR-abc123")
# => %{toast_id: "dbserver-0", role: :dbserver, ...}
```

**Implementation**: The mapping is populated after the cluster health check succeeds (agency is up, all servers registered). The `ClusterController` fetches `/_admin/cluster/health` from a coordinator and caches the mapping in its GenServer state. The mapping maps Toast server IDs (e.g., `dbserver-0`) to cluster-internal IDs (e.g., `PRMR-abc123`) and vice versa.

The mapping is stable across server restarts because the data directory is preserved (same data dir = same cluster-internal ID assigned during initial cluster formation via `ServerState.cpp`).

**Control operation targeting**: All server control operations (from Section 06) accept `cluster_id:` targeting in addition to toast server IDs and role-based targeting:

```elixir
Toast.Deployment.stop_server(deployment, cluster_id: "PRMR-abc123")
```

This enables test patterns like:
1. Query the agency to find the leader of a specific shard
2. Use the cluster-internal ID to identify which Toast server to crash
3. Verify that leadership moves to a different server after the crash

---

### 4.7 Interactive Test Execution

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/interactive.ex`

A thin wrapper for running tests from IEx against an already-running deployment:

```elixir
defmodule ToastTest.Interactive do
  @moduledoc """
  Run tests interactively against an existing deployment.

  Enables a workflow where a developer starts a deployment once in IEx
  and iterates on test code rapidly.
  """

  @doc """
  Run tests against an existing deployment.

  First argument is a module atom or file path string.
  Options:
    - deployment: (required) the Toast.Deployment handle
    - test: (optional) specific test name to run
  """
  def run(module_or_path, opts \\ [])

  # When given a file path, compile it first
  # When given a module, use it directly
  # Sets up test context, runs via ExUnit machinery, returns results
  # No deployment start/stop, no result export
end
```

**Usage from IEx**:

```elixir
{:ok, deployment} = Toast.Deployment.start(:cluster, build_dir: "/path")

# Run all tests in a module (module must be compiled/loaded)
ToastTest.Interactive.run(Smoke.VersionTest, deployment: deployment)

# Run by file path (compiles the file)
ToastTest.Interactive.run("suites/smoke/test_version.exs", deployment: deployment)

# Run a single test by name
ToastTest.Interactive.run(Smoke.VersionTest, deployment: deployment, test: "returns arango server info")
```

**Implementation details**:

1. If the first argument is a binary (file path), call `Code.compile_file/1` to compile it. The new module replaces the old one in the BEAM.
2. Store the deployment in the ETS registry via `ToastTest.DeploymentRegistry.put(suite_module, deployment)`. For interactive use where there is no suite module, use a well-known key (e.g., `:interactive`), and set up `__toast_suite__/0` to return `:interactive` on the compiled module.
3. Run the specified module's tests via ExUnit's per-test execution machinery (through the `ToastTest.ExUnitCompat` adapter). If a test name is given, filter to just that test.
4. Return results directly. No suite lifecycle, no deployment start/stop, no result packaging.

**Recompilation and ExUnit.Server**: When a file is recompiled, `use ExUnit.CaseTemplate` triggers `ExUnit.Server.add_module/2` again. Since the runner bypasses ExUnit.Server entirely, duplicate registrations are harmless. The fresh `module.__ex_unit__()` reflects the newly compiled metadata.

---

### 4.8 Inter-Suite State Cleanup

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/state_cleanup.ex`

Between suite executions, the runner calls a cleanup function that resets all shared state. This prevents one suite from leaking state into the next.

Most per-suite state that previously lived in `Application.put_env` is now held in the `%ToastTest.SuiteRun{}` struct (see section-05). The struct is local to the runner's per-suite execution — when the suite finishes, the struct simply goes out of scope. This eliminates the need to clean up 6 Application.put_env keys. The remaining cleanup targets are shared mutable state that lives outside the runner:

```elixir
defmodule ToastTest.StateCleanup do
  @moduledoc """
  Reset inter-suite shared state between suite executions.

  Most per-suite state lives in %ToastTest.SuiteRun{} and needs no cleanup
  (it goes out of scope). This module handles state that lives outside the
  runner's control flow.
  """

  def reset do
    reset_deployment_registry()
    reset_abort_table()
    reset_after_suite_callbacks()
    reset_formatters()
    # Port allocator is NOT reset — continues allocating to avoid TIME_WAIT conflicts
  end

  # ... private functions for each reset step
end
```

**What is cleaned**:

| State | How | Why |
|-------|-----|-----|
| ETS deployment registry | `ToastTest.DeploymentRegistry.clear()` | Prevents stale deployment handles from previous suite |
| ExUnit abort table (ETS `:toast_suite_abort`) | `clear_abort!()` (already exists in runner) | Previous suite's abort should not affect next suite |
| ExUnit.after_suite callbacks | Clear accumulated callbacks | `ExUnit.after_suite` callbacks accumulate globally and are never cleared by ExUnit itself — concrete bug during multi-suite execution |
| Formatter state | Stop and restart formatter GenServers | Formatters accumulate per-suite events; must start fresh |

**What is NOT cleaned**:

| State | Why NOT |
|-------|---------|
| Port allocator | Continue allocating from where it left off. Recently-released ports may still be in TCP TIME_WAIT (~60s). Reissuing them would cause bind failures for the next suite. |
| SuiteRun struct fields | Suite deadline, timeout factor, results, diagnostics, sanitizer/crash matching — all held in `%SuiteRun{}`, goes out of scope when suite finishes. No cleanup needed. |

**Optional: BEAM code purging**: After a suite completes, its test modules remain loaded in the BEAM. Over many suites, this accumulates. An optional `:code.purge/1` call for completed suites' test modules can reclaim memory. Not mandatory (memory impact is small for test modules), but the mechanism is available:

```elixir
defp purge_suite_modules(test_modules) do
  for module <- test_modules do
    :code.purge(module)
    :code.delete(module)
  end
end
```

---

### 4.9 Result Export

Files:
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter.ex` (refactored from `Toast.ResultExporter`)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter/json.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter/junit_xml.ex`

The existing `Toast.ResultExporter` is renamed to `ToastTest.ResultExporter` and extended with suite-level grouping:

**Results structure** (what goes into `results.json`):

```elixir
%{
  started_at: DateTime.t(),
  finished_at: DateTime.t(),
  global_duration_us: integer(),
  suites: [
    %{
      name: "smoke",
      suite_module: Smoke.Suite,
      deployment_mode: :single_server,
      started_at: DateTime.t(),
      finished_at: DateTime.t(),
      duration_us: integer(),
      diagnostics: map() | nil,
      tests: [
        %{
          module: Smoke.VersionTest,
          name: "returns arango server info",
          outcome: :passed | :failed | :skipped | :errored,
          duration_us: integer(),
          # ... other per-test fields
        }
      ]
    }
  ],
  summary: %{
    total: integer(),
    passed: integer(),
    failed: integer(),
    skipped: integer(),
    errored: integer()
  }
}
```

**JUnit XML**: Each suite becomes a `<testsuite>` element containing `<testcase>` elements. This gives CI systems (CircleCI) per-suite grouping in test reports.

```xml
<testsuites>
  <testsuite name="smoke" tests="4" failures="0" time="1.23">
    <testcase classname="Smoke.VersionTest" name="returns arango server info" time="0.42"/>
    ...
  </testsuite>
  <testsuite name="shell_server" tests="12" failures="1" time="5.67">
    ...
  </testsuite>
</testsuites>
```

The `ToastTest.ResultFormatter` (renamed from `Toast.ResultFormatter`) is also updated to be aware of suite boundaries. Between suites, accumulated test results are flushed to the per-suite result structure. The formatter is reset between suites as part of inter-suite cleanup (Section 4.8).

---

## File Summary

Files to **create**:

| File | Purpose |
|------|---------|
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/suite.ex` | Suite behaviour and `use` macro |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/case.ex` | Base test case template |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/process_history.ex` | Process lifecycle event observer *(deferred to Phase 4)* |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/interactive.ex` | IEx test execution |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/deployment_registry.ex` | ETS-based deployment handle registry |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/state_cleanup.ex` | Inter-suite state reset |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/suite_test.exs` | Suite behaviour tests |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/suite_discovery_test.exs` | Discovery tests |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/cli_test.exs` | CLI tests |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/case_test.exs` | Case template tests |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/process_history_test.exs` | Process history tests *(deferred to Phase 4)* |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/server_id_mapping_test.exs` | Server ID mapping tests |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/interactive_test.exs` | Interactive execution tests |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/state_cleanup_test.exs` | State cleanup tests |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast_test/result_export_test.exs` | Result export tests |

Files to **modify**:

| File | Changes |
|------|---------|
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/mix/tasks/toast.ex` | Rewrite for suite discovery, path-based CLI, `ExUnit.start(autorun: false)` early call |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter.ex` | Add suite-level grouping |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter/json.ex` | Suite-level structure in JSON output |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter/junit_xml.ex` | `<testsuite>` elements per suite |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_formatter.ex` | Suite boundary awareness |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex` | Add `cluster_id/2`, `server_by_cluster_id/2` |
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/cluster_controller.ex` | Cache cluster-internal ID mapping |

Files to **deprecate** (keep as thin wrappers):

| File | Replacement |
|------|-------------|
| `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/test_case.ex` (existing) | `ToastTest.Case` — emit compilation warning |

---

## Key Design Decisions Relevant to This Section

**Suite module IS the CaseTemplate**: The `use ToastTest.Suite` macro turns the suite module itself into a CaseTemplate. No separate module is generated. Test modules `use MySuite` directly. This keeps ownership clear (one suite per folder, test modules point to it) and avoids module-generation complexity.

**`@toast_suite` attribute AND `__toast_suite__/0` function**: The attribute enables compile-time checks. The function enables runtime discovery by the runner. Both are injected into test modules when they `use` a suite.

**`setup_deployment/1` is optional and runs once**: Checked via `function_exported?(suite_module, :setup_deployment, 1)`. The return value merges into every test's context with override semantics. This is the per-suite equivalent of `setup_all` but at the deployment level.

**No ExUnit.Server interaction**: The runner never calls `ExUnit.Server.modules_loaded/1`, `take_sync_modules/0`, or `take_async_modules/1`. Module accumulation in ExUnit.Server from compilation is harmless dead state. The runner manages its own module lists.

**Sequential suites, synchronous tests**: Suites always run one at a time. Tests within a suite always run synchronously. This ensures resource isolation and prevents inter-test interference against shared deployments.