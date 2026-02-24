Now I have comprehensive understanding of the existing code and the plan. Let me produce the section content.

# Section 5: Runner Refactoring, Crash Monitor, and Suite Migration

## Overview

This section covers the rewrite of the `ToastTest.Runner` outer scheduling layer and the extraction of the crash monitor into a standalone module. The runner is the central orchestrator: for each suite, it manages the deployment lifecycle, drives test execution, collects results, and handles abort/timeout scenarios. This section also covers migrating the existing smoke tests to the new suite structure.

**Dependencies**: This section requires section-04-suite-system to be complete. Specifically, `ToastTest.Suite` behaviour, `use ToastTest.Suite` macro, `ToastTest.Case` base template, the suite discovery mechanism in `mix toast`, and the inter-suite state cleanup logic must all exist before the runner can orchestrate per-suite execution.

**What this section produces**:
- `lib/toast_test/runner.ex` — Rewritten outer scheduling loop with per-suite orchestration
- `lib/toast_test/suite_run.ex` — Per-suite execution context struct (replaces Application.put_env)
- `lib/toast_test/exunit_compat.ex` — ExUnit internal API compatibility adapter
- `lib/toast_test/crash_monitor.ex` — Extracted crash callback module
- `lib/mix/tasks/toast.ex` — Updated mix task to drive per-suite runner execution
- `suites/smoke/suite.ex` — Smoke test suite definition
- `suites/smoke/test_version.exs` — Migrated version test
- `suites/smoke/test_collection.exs` — Migrated collection test
- `suites/smoke/test_aql.exs` — Migrated AQL test

---

## Tests

Write these tests BEFORE implementing. All tests live in `test/toast_test/` and run with `mix test` (no ArangoDB needed). Mock the deployment via `Mox` or simple stubs.

### Runner Tests (`test/toast_test/runner_test.exs`)

```elixir
defmodule ToastTest.RunnerTest do
  @moduledoc """
  Unit tests for ToastTest.Runner.

  Mock Toast.Deployment via Mox (define Toast.DeploymentBehaviour and
  Toast.MockDeployment). Mock ExUnit.EventManager interactions where needed.
  """
  use ExUnit.Case, async: false

  # Test: runner rejects modules with async: true (clear error)
  # Compile a test module with `async: true`, pass it to the runner.
  # Expect the runner to raise or return an error indicating async is not supported.

  # Test: runner executes tests in deterministic order (no shuffling)
  # Provide multiple test modules in a known order. Verify they execute
  # in that exact order, regardless of any seed value.

  # Test: runner bypasses ExUnit.Server (never calls take_sync_modules/take_async_modules)
  # Verify the runner receives modules directly from the suite orchestrator,
  # not from ExUnit.Server. Can verify by checking ExUnit.Server is never called.

  # Test: ExUnit.Server module accumulation is harmless (dead state)
  # Compile test modules that auto-register with ExUnit.Server. Verify
  # runner still works correctly since it never reads from ExUnit.Server.

  # Test: per-suite EventManager + RunnerStats created fresh (via ExUnitCompat adapter)
  # Run two suites sequentially. Verify each gets its own EventManager
  # and RunnerStats (no stats leakage between suites).

  # Test: cross-suite stats accumulator aggregates total tests, failures, duration
  # Run two suites with known test counts. Verify the final aggregated stats
  # sum tests and failures correctly.

  # Test: suite abort does not affect next suite
  # First suite aborts mid-execution. Second suite runs normally.
  # Verify second suite's tests all execute.

  # Test: deployment failure → all suite tests marked as :errored, proceeds to next
  # First suite's deployment fails to start. Verify all tests in that suite
  # are marked :errored. Second suite runs normally.

  # Test: timeout hierarchy — global deadline > suite timeout > test timeout
  # Set a global deadline that is shorter than the suite timeout.
  # Verify the suite timeout is clamped to the remaining global time.

  # Test: default suite timeout is 1 hour
  # Suite with no explicit timeout. Verify 3_600_000 ms is used.

  # Test: suite timeout clamped to remaining global deadline
  # Global deadline has 30 minutes remaining, suite timeout is 1 hour.
  # Verify the effective suite timeout is 30 minutes.

  # Test: global deadline reached mid-suite → current test aborted, remaining skipped
  # Set a very short global deadline. Run a suite with slow tests.
  # Verify the current test is aborted and remaining tests are skipped.

  # Test: drain_remaining_modules uses runner's own module list, not ExUnit.Server
  # When abort is triggered, verify remaining modules come from the runner's
  # internal list, not from ExUnit.Server.take_sync_modules.

  # Test: health check between tests rejects :degraded with clear error message
  # Mock deployment status to return :degraded after a test.
  # Verify the runner aborts the suite with a message naming downed servers.

  # Test: health check between tests rejects :failed
  # Mock deployment status to return :failed. Verify suite aborts.

  # Test: :ready status allows next test to proceed
  # Mock deployment status to return :ready. Verify next test runs.

  # Test: setup_deployment/1 returning {:error, reason} → tests marked :errored
  # Suite module where setup_deployment returns {:error, ...}.
  # Verify all tests marked :errored and deployment is shut down.

  # Test: setup_deployment/1 result merges into test context (override semantics)
  # Suite with setup_deployment that returns {:ok, %{client: custom_client}}.
  # Verify the custom client overrides the default in test context.
end
```

### Crash Monitor Tests (`test/toast_test/crash_monitor_test.exs`)

```elixir
defmodule ToastTest.CrashMonitorTest do
  @moduledoc """
  Unit tests for ToastTest.CrashMonitor — the :on_crash callback
  that bridges the deployment library to the test runner.
  """
  use ExUnit.Case, async: false

  # Test: CrashMonitor.handle_crash/2 calls Runner.abort!
  # Call handle_crash with a mock deployment and crash_info.
  # Verify Runner.abort! is called (check ETS table for abort entry).

  # Test: CrashMonitor provided as :on_crash callback to Deployment.start
  # Verify the callback function reference &CrashMonitor.handle_crash/2
  # has the correct arity and can be passed to Deployment.start options.
end
```

---

## Implementation Details

### 1. Extract CrashMonitor (`lib/toast_test/crash_monitor.ex`)

The crash monitor is currently an inline anonymous process spawned inside `Toast.Deployment` (see `spawn_crash_monitor/0` and `crash_monitor_loop/0` in the existing `deployment.ex`). It directly calls `Toast.Runner.abort!/1`, which couples the library to the test framework.

**What to build**: A module `ToastTest.CrashMonitor` that provides the `handle_crash/2` function suitable as the `:on_crash` callback.

```elixir
defmodule ToastTest.CrashMonitor do
  @moduledoc """
  Crash callback that bridges Toast.Deployment crash notifications to
  ToastTest.Runner.abort!/1. Provided as the :on_crash callback when
  the runner starts a suite's deployment.
  """

  @doc """
  Handle an unexpected server crash by aborting the current suite.

  Called by the deployment controller when an unexpected crash is detected.
  The deployment argument is the deployment struct; crash_info contains
  details about the crashed server (server_id, exit_status, signal, etc.).
  """
  @spec handle_crash(Toast.Deployment.t(), map()) :: :ok
  def handle_crash(_deployment, crash_info) do
    # Format a message from crash_info (server_id, signal, etc.)
    # and call ToastTest.Runner.abort!/1.
  end
end
```

The key insight: this module lives in `lib/toast_test/` (not `lib/toast/`) because it depends on `ToastTest.Runner`. The deployment library only knows that it receives a 2-arity function — it never imports or references this module.

**Corresponding deployment changes** (from section-02): The `spawn_crash_monitor/0`, `crash_monitor_loop/0`, and `stop_crash_monitor/1` functions are removed from `Toast.Deployment`. The `crash_monitor` field is removed from the deployment struct. Instead, `Toast.Deployment.start/2` accepts `:on_crash` as a keyword option. When the controller detects an unexpected crash, it invokes this callback (if provided). When no callback is provided (IEx use), crashes are logged but no external action is taken.

### 2. Rewrite Runner Outer Scheduling Layer (`lib/toast_test/runner.ex`)

The existing runner at `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/apps/toast/lib/toast/runner.ex` is ~870 lines, forked from ExUnit.Runner. The refactoring replaces the outer scheduling loop while preserving the per-test execution internals.

#### What to Remove

The following functions and their supporting code are deleted from the runner:

- **`async_loop/4` and `do_async_loop/4`** — The main scheduling loop that alternates between async and sync module batches. Replaced by a sequential per-suite loop.
- **`wait_until_available/2`** — Waits for spawned async modules to complete. Not needed with synchronous-only execution.
- **`spawn_modules/4`** — Spawns module groups for async execution. Replaced by direct sequential execution.
- **`maybe_store_modules/3`** — Tracks modules for repeat-until-failure. Feature removed.
- **`drain_async/2`** — Drains remaining async modules from ExUnit.Server on abort.
- **All ExUnit.Server references** — The runner never calls `ExUnit.Server.take_async_modules/1`, `ExUnit.Server.take_sync_modules/0`, or `ExUnit.Server.modules_loaded/1`.
- **`shuffle/2`** (the test-within-module shuffling) — Tests run in deterministic compilation order.

#### ExUnit Compatibility Adapter

Before modifying the runner, create a `ToastTest.ExUnitCompat` module that wraps every ExUnit internal API call: `ExUnit.RunnerStats`, `ExUnit.EventManager`, `module.__ex_unit__()`, and any other undocumented APIs. The adapter isolates version-specific assumptions into a single module. Add a compile-time check that verifies the Elixir version is within the supported range (currently 1.19+). This is the single highest-leverage action for maintainability — each Elixir version bump only requires updating the adapter, not hunting through the runner for broken internal API calls.

```elixir
defmodule ToastTest.ExUnitCompat do
  @moduledoc """
  Compatibility adapter wrapping ExUnit internal APIs.
  All runner interactions with ExUnit internals go through this module.
  """

  @supported_elixir "~> 1.19"

  # Compile-time version check
  unless Version.match?(System.version(), @supported_elixir) do
    IO.warn("ToastTest.ExUnitCompat: untested Elixir version #{System.version()}")
  end

  def start_runner_stats, do: ExUnit.RunnerStats.init(...)
  def start_event_manager, do: ExUnit.EventManager.start_link()
  def get_test_metadata(module), do: module.__ex_unit__()
  def stats(stats_pid), do: ExUnit.RunnerStats.stats(stats_pid)
  # ... wrap all other ExUnit internal calls
end
```

#### What to Preserve

The following per-test execution internals remain largely unchanged (but accessed through `ExUnitCompat`):

- **`run_module/5`** — Runs all tests in a single module (setup_all, per-test loop, teardown). This is the core execution unit.
- **`prepare_tests/4`** — Filters tests by include/exclude tags. Keep but remove the `shuffle` call within it.
- **`spawn_test/3`** and **`spawn_test_monitor/4`** — Spawns an individual test in a monitored process with capture_log, tmp_dir, setup/test/on_exit lifecycle.
- **`receive_test_reply/4`** — Handles test completion with timeout.
- **`exec_test_setup/2`**, **`exec_test/2`**, **`exec_on_exit/3`** — Individual test execution steps.
- **`run_setup_all/4`** — Module-level setup_all with its spawned process pattern.
- **`abort!/1`**, **`clear_abort!/0`**, **`aborted?/0`** — ETS-based abort mechanism.
- **`check_suite_deadline!/1`** and **`clamp_to_deadline/2`** — Timeout clamping.
- **`prune_stacktrace/1`** — Stacktrace cleanup.
- **`get_timeout/2`** — Timeout resolution with factor and clamping.
- **sigquit handling** — SIGQUIT trap for diagnostic dump.

#### New Outer Loop: `run_suites/2`

The new entry point replaces the monolithic `run/2`. The runner now orchestrates per-suite execution:

```elixir
@spec run_suites(list(suite_entry()), keyword()) :: aggregated_stats()
def run_suites(suites, global_opts) do
  # suites is a list of %{module: SuiteModule, test_modules: [Module1, Module2, ...]}
  # global_opts includes :global_deadline, :formatters, etc.

  # For each suite, sequentially:
  #   1. Resolve deployment mode (:auto → actual mode)
  #   2. Compute suite deadline (clamp suite timeout to remaining global time)
  #   3. Start deployment with :on_crash callback and :on_event callback
  #   4. Handle deployment failure → mark all tests as :errored, proceed
  #   5. Run optional setup_deployment/1 callback
  #   6. Handle setup failure → mark all tests as :errored, stop deployment, proceed
  #   7. Validate no async modules (reject with error)
  #   8. Create fresh EventManager and RunnerStats via ExUnitCompat adapter
  #   9. Attach formatters to EventManager
  #  10. Run tests via run_module for each test module
  #  11. Between tests: check deployment health (reject :degraded and :failed)
  #  12. Run optional teardown_deployment/1 callback
  #  13. Call stop_and_collect to gather diagnostics
  #  14. Extract suite stats, merge into cross-suite accumulator
  #  15. Clean up inter-suite state (Section 4.8)
  #  16. Clear abort state for next suite
end
```

#### Async Rejection

Before running a suite's tests, the runner validates that none of the test modules have `async: true`:

```elixir
defp validate_no_async!(test_modules) do
  async_modules =
    test_modules
    |> Enum.filter(fn mod ->
      case mod.__ex_unit__() do
        %{tags: %{async: true}} -> true
        _ -> false
      end
    end)

  if async_modules != [] do
    names = Enum.map_join(async_modules, ", ", &inspect/1)
    raise "Toast does not support async test modules. Found: #{names}"
  end
end
```

#### Health Check Between Tests

The existing runner already checks deployment health between tests via `check_deployments/1`. This is preserved and extended to support per-suite configuration via the `between_tests` option on `use ToastTest.Suite`:

```elixir
defp check_between_tests(suite_run, test) do
  suite_module = suite_run.suite_module
  config = suite_module.deployment_config()

  case Keyword.get(config, :between_tests, :default) do
    false ->
      :ok

    :default ->
      check_deployment_health(suite_run.deployment)

    callback when is_function(callback, 2) ->
      callback.(suite_run.deployment, test)
  end
end

defp check_deployment_health(deployment) do
  case Toast.Deployment.status(deployment) do
    :ready ->
      :ok

    :degraded ->
      down_servers =
        deployment
        |> Toast.Deployment.servers()
        |> Enum.filter(fn {_id, state} -> state.operational_state in [:stopped, :paused, :killed] end)
        |> Enum.map(fn {id, _state} -> id end)

      {:error,
       "Deployment is degraded after test — servers #{inspect(down_servers)} are still down. " <>
         "Tests must restore all servers before finishing."}

    :failed ->
      {:error, format_crash_message(Toast.Deployment.crash_info(deployment))}

    other ->
      {:error, "Deployment not ready (status: #{other})"}
  end
end
```

Suites can also implement the `between_tests/2` behaviour callback, which the runner resolves via `function_exported?`. The three modes:
- **`:default`** (omitted): Check deployment status is `:ready` — rejects `:degraded` and `:failed`
- **`false`**: Skip check entirely — fast suites with no server manipulation
- **Custom callback**: Per-suite logic for suites with specific health check needs

This check runs in `run_tests_loop` between each test, exactly where the current `check_deployments/1` call lives.

#### Suite-Level Timeout

Each suite has a timeout (default 1 hour, configurable via `timeout:` in `use ToastTest.Suite`). The runner computes the effective suite deadline before starting a suite:

```elixir
defp compute_suite_deadline(suite_timeout, global_deadline) do
  now = System.monotonic_time(:millisecond)
  suite_end = now + suite_timeout

  case global_deadline do
    nil -> suite_end
    deadline -> min(suite_end, deadline)
  end
end
```

This deadline is stored in the `%ToastTest.SuiteRun{}` struct (see below) and accessed by the existing `check_suite_deadline!/1` and `clamp_to_deadline/2` functions.

#### Cross-Suite Stats Aggregation

Each suite gets its own `ExUnit.EventManager` and `ExUnit.RunnerStats` (accessed through the `ToastTest.ExUnitCompat` adapter). After each suite completes:

```elixir
defp merge_stats(accumulator, suite_stats) do
  %{
    total: accumulator.total + suite_stats.total,
    failures: accumulator.failures + suite_stats.failures,
    skipped: accumulator.skipped + suite_stats.skipped,
    excluded: accumulator.excluded + suite_stats.excluded
  }
end
```

The accumulator starts as `%{total: 0, failures: 0, skipped: 0, excluded: 0}` and grows as each suite finishes.

#### SuiteRun Context Struct

File: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/suite_run.ex`

Per-suite mutable state that previously lived in `Application.put_env` is now held in a `%ToastTest.SuiteRun{}` struct threaded through the runner's per-suite execution:

```elixir
defmodule ToastTest.SuiteRun do
  @moduledoc """
  Per-suite execution context. Created by the runner for each suite,
  threaded through execution functions, and discarded when the suite finishes.
  No cleanup needed — the struct is local, not global.
  """

  defstruct [
    :suite_module,
    :deployment,
    :suite_deadline,
    :timeout_factor,
    results: [],
    diagnostics: nil,
    sanitizer_matching: %{},
    crash_matching: %{}
  ]

  @type t :: %__MODULE__{
    suite_module: module(),
    deployment: Toast.Deployment.t() | nil,
    suite_deadline: integer(),
    timeout_factor: float(),
    results: [map()],
    diagnostics: map() | nil,
    sanitizer_matching: map(),
    crash_matching: map()
  }
end
```

The runner creates one per suite and threads it through `run_module/5` (via the existing `config` parameter). When the suite finishes, the struct goes out of scope — no cleanup needed. This replaces the 6 `Application.put_env(:toast, key)` keys (`:__suite_deadline__`, `:__timeout_factor__`, `:__test_results__`, `:__test_diagnostics__`, `:__sanitizer_matching__`, `:__crash_matching__`) with a local, explicit data structure.

Deployment delivery stays in the ETS registry (`ToastTest.DeploymentRegistry`) — it must be readable by test setup callbacks running in spawned processes.

#### Deployment Failure Handling

If `Toast.Deployment.start/2` returns `{:error, reason}` for a suite, all tests in that suite are marked as `:errored`:

```elixir
defp mark_all_errored(test_modules, reason, manager) do
  for module <- test_modules do
    test_module = module.__ex_unit__()
    EM.module_started(manager, test_module)

    for test <- test_module.tests do
      errored = %{test | state: {:failed, [{:error, reason, []}]}}
      EM.test_started(manager, errored)
      EM.test_finished(manager, errored)
    end

    EM.module_finished(manager, test_module)
  end
end
```

The runner then proceeds to the next suite.

#### `setup_deployment/1` Integration

After the deployment starts successfully, the runner checks if the suite module exports `setup_deployment/1`:

```elixir
defp run_suite_setup(suite_module, deployment) do
  if function_exported?(suite_module, :setup_deployment, 1) do
    suite_module.setup_deployment(deployment)
  else
    {:ok, %{}}
  end
end
```

If it returns `{:ok, extra_context}`, the extra context is stored alongside the deployment in the ETS registry and merged into every test's context by `ToastTest.Case.setup`. Keys in extra context override the base context.

If it returns `{:error, reason}`, all tests are marked `:errored` and the deployment is shut down via `stop_and_collect/1`.

### 3. Update Mix Task (`lib/mix/tasks/toast.ex`)

The mix task changes to drive per-suite execution rather than a single flat run. The key changes:

**Before (current)**:
1. Parse CLI args
2. Set env vars via `apply_toast_env`
3. Load test_helper.exs (which calls `ExUnit.start()` and `setup_suite!()`)
4. Find and compile test files
5. Call `ExUnit.Server.modules_loaded`
6. Call `Toast.Runner.run(options, load_us)`

**After (new)**:
1. Parse CLI args, including path-based suite selection
2. Read env vars and config ONCE into keyword opts (no `System.put_env` calls)
3. Call `ExUnit.start(autorun: false)` early, before any compilation
4. Discover suite folders in `suites/`
5. Compile all `suite.ex` files globally
6. Identify requested suites (from CLI paths or all)
7. Build suite entries: for each suite, list its test modules (not yet compiled)
8. Call `ToastTest.Runner.run_suites(suite_entries, global_opts)`
9. The runner handles per-suite compilation of helpers and test files, deployment lifecycle, and test execution
10. Process aggregated results and exit codes

The env var reading is refactored: instead of `apply_toast_env` calling `System.put_env`, the task reads `TOAST_*` env vars once and passes them as keyword opts. This prevents env var pollution between suites.

```elixir
defp read_toast_config(cli_opts) do
  # Read env vars once, merge with CLI opts (CLI takes precedence)
  env_opts = read_env_vars()
  local_opts = read_local_config()
  # Precedence: cli_opts > env_opts > local_opts > defaults
  Keyword.merge(defaults(), local_opts)
  |> Keyword.merge(env_opts)
  |> Keyword.merge(cli_opts)
end
```

### 4. Migrate Smoke Tests to Suite Structure

The existing smoke tests in `apps/smoke_test/test/smoke_test/` are migrated to the new suite structure under `suites/smoke/`.

#### Suite Definition (`suites/smoke/suite.ex`)

```elixir
defmodule Smoke.Suite do
  @moduledoc "Smoke test suite — basic connectivity and CRUD verification."
  use ToastTest.Suite
end
```

This is the minimal suite definition. It uses `mode: :auto` (the default), so it runs against whatever deployment mode is configured globally (single server or cluster).

#### Migrated Test Files

Each existing test file is renamed from `*_test.exs` to `test_*.exs` and updated to `use Smoke.Suite` instead of `use Toast.TestCase`.

**`suites/smoke/test_version.exs`** (from `apps/smoke_test/test/smoke_test/version_test.exs`):

```elixir
defmodule Smoke.VersionTest do
  use Smoke.Suite

  test "returns arango server info", %{client: client} do
    # ... (preserved from existing test)
  end

  test "endpoint is accessible via raw HTTP", %{endpoint: endpoint} do
    # ... (preserved)
  end

  # ... remaining tests
end
```

Changes from existing:
- `use Toast.TestCase` → `use Smoke.Suite`
- Module name `SmokeTest.VersionTest` → `Smoke.VersionTest`
- `Client.version(client)` calls depend on section-03 client refactoring. During migration, keep the existing `Toast.Client` function calls if the new domain modules are not yet available.

**`suites/smoke/test_collection.exs`** (from `collection_test.exs`):
- Same pattern: `use Smoke.Suite`, module rename to `Smoke.CollectionTest`

**`suites/smoke/test_aql.exs`** (from `aql_test.exs`):
- Same pattern: `use Smoke.Suite`, module rename to `Smoke.AqlTest`

The `test_helper.exs` file in `apps/smoke_test/test/` is deleted. Its responsibilities (starting ExUnit, starting the deployment) are now handled by the mix task and runner respectively.

### 5. Per-Suite Execution Flow (Detailed)

This is the complete flow for a single suite within `run_suites/2`:

1. **Compile suite helpers**: Compile `*.ex` files (excluding `suite.ex`, already compiled) in the suite's folder.

2. **Compile test files**: Compile `test_*.exs` files in the suite's folder. These modules auto-register with ExUnit.Server (harmless).

3. **Collect test modules**: Find all compiled modules whose `__toast_suite__/0` returns this suite module.

4. **Validate**: Reject any modules with `async: true`.

5. **Resolve mode**: Read `deployment_config/0`, resolve `:auto` to actual mode.

6. **Compute deadline**: `compute_suite_deadline(suite_timeout, global_deadline)`.

7. **Create SuiteRun**: Build `%ToastTest.SuiteRun{suite_module: suite_module, suite_deadline: deadline, timeout_factor: factor}`. This struct is threaded through execution functions and replaces `Application.put_env` for suite-local state.

8. **Start deployment**: `Toast.Deployment.start(mode, deployment_opts)` where `deployment_opts` includes `:on_crash` pointing to `&ToastTest.CrashMonitor.handle_crash/2`. The `:on_event` callback is omitted in Phase 3b (ProcessHistory is deferred to Phase 4).

9. **Handle start failure**: If `{:error, reason}`, mark all tests `:errored`, skip to cleanup. For partially-started deployments (e.g., some servers started but health checks failed), `stop_and_collect/1` must handle gracefully — only collect diagnostics from servers that actually started.

10. **Store deployment**: `ToastTest.DeploymentRegistry.put(suite_module, deployment)` — stores in ETS registry keyed by suite module (replaces `Application.put_env`).

11. **Run setup_deployment**: Call optional `setup_deployment/1`. On error, mark tests `:errored`, call `stop_and_collect`, skip to cleanup.

12. **Store suite context**: Store extra context alongside the deployment in the registry.

13. **Create EventManager**: `{:ok, manager} = ToastTest.ExUnitCompat.start_event_manager()`. Add RunnerStats and formatters via the adapter.

14. **Emit suite_started**: `EM.suite_started(manager, opts)`.

15. **Run modules**: For each test module, call `run_module(config, module, false, nil, %{})`. Between tests, call `check_between_tests(suite_run, test)` which dispatches based on the suite's `between_tests` configuration (`:default` → status check, `false` → skip, custom callback → delegate). On failure, abort the suite (only this suite's remaining tests skipped).

16. **Emit suite_finished**: `EM.suite_finished(manager, times_us)`.

17. **Extract stats**: `ToastTest.ExUnitCompat.stats(stats_pid)`. Merge into accumulator.

18. **Run teardown_deployment**: Call optional `teardown_deployment/1` if exported.

19. **Collect diagnostics**: `Toast.Deployment.stop_and_collect(deployment)`.

20. **Clean up**: Reset inter-suite state (section 4.8): clear ETS deployment registry, clear abort ETS table, clear ExUnit.after_suite callbacks, reset formatter GenServers. SuiteRun struct goes out of scope (no cleanup needed). Port allocator is NOT reset. Optionally purge completed suite's test modules via `:code.purge/1`.

21. **Proceed**: Move to next suite.

### 6. Key Design Constraints

**No async execution**: The runner enforces `async: false` for all test modules. This eliminates all async scheduling complexity (the existing `async_loop`, `do_async_loop`, `wait_until_available`, `spawn_modules` are all removed). Tests against a shared deployment must be sequential to avoid interference.

**No test shuffling**: Tests run in deterministic compilation order within a module. The `shuffle/2` function within `prepare_tests` is removed. Module execution order is also deterministic (compilation order within the suite folder).

**ExUnit.Server is bypassed**: The runner never calls `ExUnit.Server.take_async_modules/1` or `ExUnit.Server.take_sync_modules/0`. Modules auto-registering with ExUnit.Server is harmless dead state. The `drain_remaining_modules/2` function is rewritten to drain from the runner's own module list rather than from ExUnit.Server.

**Sequential suites**: Suites run one at a time. A suite's deployment is fully stopped before the next suite's deployment starts. This ensures port isolation and simplifies resource management.

**Abort scope**: `abort!/1` affects only the current suite. The `clear_abort!/0` call between suites resets the ETS table so the next suite starts clean.

### 7. Migration Plan Alignment

This section corresponds to Phase 3b, steps 3-6 and 11-14 from the migration plan:

- **Step 3**: Rewrite runner outer scheduling layer (remove async, remove shuffling, bypass ExUnit.Server)
- **Step 4**: Refactor runner for per-suite execution with deployment lifecycle, inter-suite cleanup, deployment failure handling, degraded rejection
- **Step 5**: Implement suite discovery in mix toast task
- **Step 6**: Migrate smoke tests to new suite structure
- **Step 11**: Implement path-based CLI
- **Step 12**: Add orphan file detection
- **Step 13**: Implement `ToastTest.Interactive.run/2`

ProcessHistory is deferred to Phase 4 (section-07). Steps 11-13 may overlap with section-04. The core deliverable of this section is steps 3-6 — the runner rewrite and smoke test migration.

### 8. Verification

After implementation, verify:

1. `mix test` — Framework unit tests pass (including new runner and crash monitor tests)
2. `mix toast smoke` — Smoke tests run against a real deployment using the new suite system
3. `mix toast` — All suites discovered and run sequentially
4. A deployment failure in one suite does not prevent subsequent suites from running
5. An abort in one suite (simulated via the crash monitor callback) does not affect the next suite
6. The global timeout properly clamps suite timeouts
7. `mix xref graph` — `lib/toast/` has no reference to `ToastTest.Runner` or `ToastTest.CrashMonitor`